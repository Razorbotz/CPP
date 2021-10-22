#define Phoenix_No_WPI // remove WPI dependencies
#include "ctre/Phoenix.h"
#include "ctre/phoenix/platform/Platform.h"


int main(int argc,char** argv){
        ctre::phoenix::platform::can::SetCANInterface("can0");
        TalonSRX* talonSRX=new TalonSRX(10);
        talonSRX->Set(ControlMode::PercentOutput, .10);

	while(true){

	}
}

