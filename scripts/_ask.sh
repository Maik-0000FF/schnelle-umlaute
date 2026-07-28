# shellcheck shell=bash
# Shared interactive prompt for install.sh and uninstall.sh.
# Source from a script that has already set "set -e" if desired.
#
# After calling ask, the caller can rely on:
#   $REPLY   — the answer, empty when there was none (EOF, or a bare Enter)
#
# Both scripts run under "set -e", where a bare `read` is a trap: it returns
# non-zero at end of input, so a piped or otherwise non-interactive run
# (`curl ... | bash`, a CI job, a closed stdin) aborts the script mid-way
# instead of falling through to the prompt's default. ask() absorbs that: an
# EOF simply produces an empty answer, which every call site already treats as
# "take the default". Keeping it in one place stops the two scripts from
# drifting on a detail that fails silently in exactly the runs nobody watches.
ask() {
    # Cleared first so an EOF cannot leave the previous prompt's answer in
    # place, which would silently apply one question's reply to the next.
    REPLY=""
    read -r -p "$1" && return
    # No answer at all. The empty REPLY makes the caller take the default its
    # prompt advertises, which for a [Y/n] question means yes and can mean
    # removing files or overwriting a config. Say so, so an unattended run
    # leaves a record that the default was taken FOR the user instead of a log
    # that reads as if someone had answered.
    #
    # The question is repeated here on purpose: `read -p` only writes its
    # prompt when stdin is a terminal, so in exactly the run this message
    # exists for, the prompt itself never reaches the log. Without it a script
    # with several prompts leaves a column of identical, unattributable lines.
    echo
    echo "No input received for: $1"
    echo "Taking the default."
}
