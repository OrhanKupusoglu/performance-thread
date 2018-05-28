#!/bin/bash

# exit immediately on errors
set -e

# read arguments
ACTION="$1"
COMMAND="$2"

# parameter expansion - to clear white space
ACTION=${ACTION/ //}
COMMAND=${COMMAND/ //}

# get working dir
DIR_WORK=$(cd $(dirname "$0") && pwd)

# test application
APP=prf-system-app

# helper functions
help () {
    echo "USAGE: enter a command, no command defaults to 'build'"
    echo "    build         -- call 'lib cmake; lib make; app cmake; app make;'"
    echo "    clean         -- call 'lib clean; app clean;'"
    echo "    prune         -- call 'lib prune; app prune;'"
    echo "    purge         -- call 'clean; prune;'"
    echo "    run           -- run the test executable"
    echo "    help          -- print this help"
    echo "    lib cmake     -- call 'cmake'"
    echo "    lib make      -- call 'make; make install'"
    echo "    lib clean     -- call 'make clean'"
    echo "    lib prune     -- prune all artifacts"
    echo "    app cmake     -- call 'cmake'"
    echo "    app make      -- call 'make'"
    echo "    app clean     -- call 'make clean'"
    echo "    app prune     -- prune all artifacts"
}

action() {
    local PROJ=$1
    local CMD=$2
    local DIR=""
    local ACTION_ING=""
    local ACTION_ED=""
    local INST=""
    local PRUNE=""

    case "$PROJ" in
        "lib")      DIR="library" ; INST="make install" ; PRUNE="cd $DIR_WORK/$DIR/lib ; find . ! -name '.gitignore' -delete" ;;
        "app")      DIR="application" ;;
        *)          echo "ERROR: argument proj '${PROJ}' is not supported" ; exit 1 ;;
    esac

    case "$CMD" in
        "cmake")    ACTION_ING="CMaking" ; ACTION_ED="CMaked" ;;
        "make")     ACTION_ING="Making" ; ACTION_ED="Maked" ;;
        "clean")    ACTION_ING="Cleaning" ; ACTION_ED="Cleaned" ;;
        "prune")    ACTION_ING="Pruning" ; ACTION_ED="Pruned" ; ;;
        *)          echo "ERROR: argument command '${CMD}' is not supported" ; exit 2 ;;
    esac

    cd $DIR_WORK/$DIR/build

    echo "++ ${ACTION_ING} the ${DIR} project ..."

    case "$CMD" in
        "cmake")      cmake ../. ;;
        "make")       make ; eval $INST ;;
        "clean")      make clean ;;
        "prune")      find . ! -name '.gitignore' -delete ; eval $PRUNE ;;
    esac

    echo "++ ${ACTION_ED} the ${DIR} project."
}

# compound commands
build() {
    action lib cmake
    action lib make
    action app cmake
    action app make
}

clean() {
    action lib clean
    action app clean
}

prune() {
    action lib prune
    action app prune
}

purge() {
    clean
    prune
}

run() {
    cd $DIR_WORK/application/bin

    if [[ -e ${APP} ]]
    then
        ./${APP}
    else
        echo "ERROR: missing executable '${APP}'"
        exit 3
    fi
}

# action
case "$ACTION" in
    "build")        build ; exit 0 ;;
    "clean")        clean ; exit 0 ;;
    "prune")        prune ; exit 0 ;;
    "purge")        purge ; exit 0 ;;
    "run")          run ; exit 0 ;;
    "help")         help ; exit 0 ;;
    "lib"|"app")    action "$ACTION" "$COMMAND" ; exit 0 ;;
    "")             build ; exit 0 ;;
    *)              echo "ERROR: command '${ACTION}' is not supported" ; help ; exit 4 ;;
esac
