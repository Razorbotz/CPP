# Bash tab-completion for the Razorbotz control program.
# Usage: source control_completion.sh   (or add that line to ~/.bashrc)
# Keep this list in sync with processArguments() in src/control.cpp.
_control_completions()
{
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Flags that take a file argument: complete file names
    case "$prev" in
        --config_file|--input_config)
            COMPREPLY=( $(compgen -f -- "$cur") )
            return 0
            ;;
    esac

    local opts="--help --init --no_video --set_colors --wsl --config_file --nano \
--test_input --alt_layout --input_config --simulate --encode_tool \
--backup_bot --dump_bot --debug_glade_bounds --disable_foxglove --debug_motors \
--fe --flight_engineer --forward --no_forward --mission_time \
--battery_capacity --max_bandwidth"

    COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
}
complete -F _control_completions ./control