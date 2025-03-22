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

if [ -n "${BUILDENV_ARM64}" ]; then
    BUILDENV_NAME="mixxx-deps-2.6-arm64-osx-min1100-release-557befb"
    BUILDENV_ID="2796308869"
    VCPKG_TARGET_TRIPLET="arm64-osx-min1100-release"
else
    BUILDENV_NAME="mixxx-deps-2.6-x64-osx-min1100-557befb"
    BUILDENV_ID="2796464048"
    VCPKG_TARGET_TRIPLET="x64-osx-min1100"
fi

MIXXX_ROOT="$(realpath "$(dirname "$THIS_SCRIPT_NAME")/..")"

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
                if curl -o "${BUILDENV_PATH}_.zip" -L -H "Accept: application/vnd.github+json" -H "Authorization: Bearer $2" -H "X-GitHub-Api-Version: 2022-11-28" https://api.github.com/repos/daschuer/vcpkg/actions/artifacts/${BUILDENV_ID}/zip; then
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

        export MIXXX_VCPKG_ROOT="${BUILDENV_PATH}"
        export CMAKE_GENERATOR=Ninja
        export VCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}"

        echo_exported_variables() {
            echo "MIXXX_VCPKG_ROOT=${MIXXX_VCPKG_ROOT}"
            echo "CMAKE_GENERATOR=${CMAKE_GENERATOR}"
            echo "VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
        }

        if [ -n "${GITHUB_ENV}" ]; then
            echo_exported_variables >> "${GITHUB_ENV}"
        elif [ "$1" != "--profile" ]; then
            echo ""
            echo "Exported environment variables:"
            echo_exported_variables
            echo "You can now configure cmake from the command line in an EMPTY build directory via:"
            echo "cmake -DCMAKE_TOOLCHAIN_FILE=${MIXXX_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake ${MIXXX_ROOT}"
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
