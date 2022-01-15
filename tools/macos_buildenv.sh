#!/bin/bash
set -o pipefail

# shellcheck disable=SC2091
if [ -z "${GITHUB_ENV}" ] && ! $(return 0 2>/dev/null); then
  echo "This script must be run by sourcing it:"
  echo "source $0 $*"
  exit 1
fi

realpath() {
    OLDPWD="${PWD}"
    cd "$1" || exit 1
    pwd
    cd "${OLDPWD}" || exit 1
}

# some hackery is required to be compatible with both bash and zsh
THIS_SCRIPT_NAME=${BASH_SOURCE[0]}
[ -z "$THIS_SCRIPT_NAME" ] && THIS_SCRIPT_NAME=$0

MIXXX_ROOT="$(realpath "$(dirname "$THIS_SCRIPT_NAME")/..")"

read -r -d'\n' BUILDENV_NAME BUILDENV_SHA256 < "${MIXXX_ROOT}/packaging/macos/build_environment"
BUILDENV_NAME="mixxx-deps-2.4-arm64-osx-min11.0-85f2803"

[ -z "$BUILDENV_BASEPATH" ] && BUILDENV_BASEPATH="${MIXXX_ROOT}/buildenv"

case "$1" in
    name)
        if [ -n "${GITHUB_ENV}" ]; then
            echo "BUILDENV_NAME=$BUILDENV_NAME" >> "${GITHUB_ENV}"
        else
            echo "$BUILDENV_NAME"
        fi
        ;;

    setup)
        BUILDENV_PATH="${BUILDENV_BASEPATH}/${BUILDENV_NAME}"
        mkdir -p "${BUILDENV_BASEPATH}"
        if [ ! -d "${BUILDENV_PATH}" ]; then
            if [ "$1" != "--profile" ]; then
                echo "Build environment $BUILDENV_NAME not found in mixxx repository, downloading it..."
                if curl -o "${BUILDENV_PATH}_.zip" -L -H "authorization: token $2" -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/daschuer/vcpkg/actions/artifacts/143040365/zip; then
                    #OBSERVED_SHA256=$(shasum -a 256 "${BUILDENV_PATH}.zip"|cut -f 1 -d' ')
                    #if [[ "$OBSERVED_SHA256" == "$BUILDENV_SHA256" ]]; then
                    #    echo "Download matched expected SHA256 sum $BUILDENV_SHA256"
                    #else
                    #    echo "ERROR: Download did not match expected SHA256 checksum!"
                    echo "Expected $BUILDENV_SHA256"
                    #    echo "Got $OBSERVED_SHA256"
                    #    exit 1
                    #fi
                    echo "Extracting ${BUILDENV_NAME}_.zip..."
                    unzip "${BUILDENV_PATH}_.zip" -d "${BUILDENV_BASEPATH}" && \
                    echo "Successfully extracted ${BUILDENV_NAME}_.zip" && \
                    rm "${BUILDENV_PATH}_.zip"
                    echo "Extracting ${BUILDENV_NAME}.zip..."
                    unzip "${BUILDENV_PATH}.zip" -d "${BUILDENV_BASEPATH}" && \
                    echo "Successfully extracted ${BUILDENV_NAME}.zip" && \
                    rm "${BUILDENV_PATH}.zip"
                else
                   echo "Downloading build environment failed"
                fi
            else
                echo "Build environment $BUILDENV_NAME not found in mixxx repository, run the command below to download it."
                echo "source ${THIS_SCRIPT_NAME} setup"
                return # exit would quit the shell being started
            fi
        elif [ "$1" != "--profile" ]; then
            echo "Build environment found: ${BUILDENV_PATH}"
        fi

        export VCPKG_ROOT="${BUILDENV_PATH}"
        export CMAKE_GENERATOR=Ninja

        echo_exported_variables() {
            echo "VCPKG_ROOT=${VCPKG_ROOT}"
            echo "CMAKE_GENERATOR=${CMAKE_GENERATOR}"
        }

        if [ -n "${GITHUB_ENV}" ]; then
            echo_exported_variables >> "${GITHUB_ENV}"
        elif [ "$1" != "--profile" ]; then
            echo ""
            echo "Exported environment variables:"
            echo_exported_variables
        fi
        ;;
    *)
        echo "Usage: source macos_buildenv.sh [options]"
        echo ""
        echo "options:"
        echo "   help       Displays this help."
        echo "   name       Displays the name of the required build environment."
        echo "   setup      Installs the build environment."
        ;;
esac
