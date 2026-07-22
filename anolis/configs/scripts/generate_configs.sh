#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# Generate the whole kconfig files.
#
# Copyright (C) 2023 Qiao Ma <mqaio@linux.alibaba.com>

set -e -o pipefail

SCRIPT_DIR=$(realpath $(dirname $0))
FILE_LIST=${DIST_OUTPUT}/file_list

mkdir -p ${DIST_OUTPUT}

if [ -z "$@" ]; then
    python3 ${SCRIPT_DIR}/anolis_kconfig.py generate_translate \
        --input_dir ${SCRIPT_DIR}/../ \
        --output_dir ${DIST_OUTPUT} \
        --src_root ${DIST_SRCROOT} \
        ${DIST_SRCROOT}/${DIST_CONFIG_LAYOUTS} > ${DIST_OUTPUT}/generate.sh
else
    for target in $@
    do
    python3 ${SCRIPT_DIR}/anolis_kconfig.py generate_translate \
        --input_dir ${SCRIPT_DIR}/../ \
        --output_dir ${DIST_OUTPUT} \
        --src_root ${DIST_SRCROOT} \
        --target ${DIST_CONFIG_KERNEL_NAME}/${target} \
        ${DIST_SRCROOT}/${DIST_CONFIG_LAYOUTS} > ${DIST_OUTPUT}/generate.sh
    done
fi

export DIST_KBUILD_OUTPUT DIST_CONFIG_PATH
bash "${DIST_OUTPUT}/generate.sh" | tee "${FILE_LIST}"

if [ "x${DIST_DO_GENERATE_DOT_CONFIG}" = "xY" ]; then
    file=$(awk '/processed/ { print $4; exit }' "${FILE_LIST}")
    if [ -z "${file}" ] || [ ! -f "${file}" ]; then
        echo "Unable to find the generated configuration file" >&2
        exit 1
    fi

    config_path=${DIST_CONFIG_PATH:-${DIST_SRCROOT}.config}
    mkdir -p "$(dirname "${config_path}")"
    if ! cmp -s "${file}" "${config_path}"; then
        cp -f "${file}" "${config_path}"
    fi
fi
