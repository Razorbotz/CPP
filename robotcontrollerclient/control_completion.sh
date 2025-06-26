_control_completions()
{
    local cur="${COMP_WORDS[COMP_CWORD]}"
    COMPREPLY=( $(compgen -W "--init --no_video --no_arena --set_colors --set_map --wsl --config_file --help" -- "$cur") )
}
complete -F _control_completions ./control
