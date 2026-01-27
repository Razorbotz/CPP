import cv2
import numpy as np

# Capture one frame from each camera (adapt these paths as needed)
zed_cap = cv2.VideoCapture("/dev/video0")      # or use ZED SDK grab
rs_cap  = cv2.VideoCapture("/dev/video2")      # RealSense RGB camera

ret1, img_zed = zed_cap.read()
ret2, img_rs  = rs_cap.read()
zed_cap.release()
rs_cap.release()

if not (ret1 and ret2):
    raise RuntimeError("Could not capture from both cameras")

# Resize both so heights match
h_target = 720
scale1 = h_target / img_zed.shape[0]
scale2 = h_target / img_rs.shape[0]
zed  = cv2.resize(img_zed, None, fx=scale1, fy=scale1)
rs   = cv2.resize(img_rs, None, fx=scale2, fy=scale2)

# Convert to gray for feature detection
zed_gray = cv2.cvtColor(zed, cv2.COLOR_BGR2GRAY)
rs_gray  = cv2.cvtColor(rs,  cv2.COLOR_BGR2GRAY)

# Detect and match features
orb = cv2.ORB_create(2000)
kp1, des1 = orb.detectAndCompute(zed_gray, None)
kp2, des2 = orb.detectAndCompute(rs_gray, None)
matcher = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
matches = matcher.match(des1, des2)

# Keep good matches
matches = sorted(matches, key=lambda x: x.distance)
good = matches[:int(len(matches) * 0.15)]

pts1 = np.float32([kp1[m.queryIdx].pt for m in good])
pts2 = np.float32([kp2[m.trainIdx].pt for m in good])

H, mask = cv2.findHomography(pts2, pts1, cv2.RANSAC)

print("Homography matrix:\n", H)
cv2.imwrite("zed_rs_matches.jpg",
            cv2.drawMatches(zed, kp1, rs, kp2, good, None,
                            flags=cv2.DrawMatchesFlags_NOT_DRAW_SINGLE_POINTS))

# Save as YAML
fs = cv2.FileStorage("homography.yaml", cv2.FILE_STORAGE_WRITE)
fs.write("H", H)
fs.release()
print("Saved to homography.yaml")
