#!/bin/bash
#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017-2020 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

function blob_fixup() {
    # Camera
    if [[ "${1}" =~ ^odm/overlayfs/.*/lib/libmmcamera.*\.so$ ]]; then
        sed -i 's|data/misc/camera|data/vendor/qcam|g;s|libgui.so|libwui.so|g' "${2}"
    fi

    case "${1}" in
        # Camera
        odm/overlayfs/*/bin/mm-qcamera-daemon)
            sed -i 's|data/misc/camera|data/vendor/qcam|g' "${2}"
            ;;
        odm/overlayfs/*/lib/libmmcamera_ppeiscore.so)
            if ! "${PATCHELF}" --print-needed "${2}" | grep "libshims_ui.so" > /dev/null; then
                "${PATCHELF}" --add-needed "libshims_ui.so" "${2}"
            fi
            ;;
        odm/overlayfs/*/lib/libmmcamera2_sensor_modules.so)
            sed -i 's|/system/etc/camera/|////odm/etc/camera/|g' "${2}"
            sed -i 's|/system/vendor/lib|////vendor/odm/lib|g' "${2}"
            ;;
        odm/overlayfs/*/lib/libmmcamera2_stats_modules.so)
            "${PATCHELF}" --replace-needed "libandroid.so" "libshims_android.so" "${2}"
            ;;
        odm/overlayfs/*/lib/libmmsw_platform.so|odm/overlayfs/*/lib/libmmsw_detail_enhancement.so)
            "${PATCHELF}" --remove-needed "libbinder.so" "${2}"
            sed -i 's|libgui.so|libwui.so|g' "${2}"
            ;;
        odm/overlayfs/*/lib/libmpbase.so)
            "${PATCHELF}" --replace-needed "libandroid.so" "libshims_android.so" "${2}"
            ;;
    esac

    # For all ELF files
    if [[ "${1}" =~ ^.*(\.so|\/bin\/.*)$ ]]; then
        "${PATCHELF}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${2}"
    fi
}

# If we're being sourced by the common script that we called,
# stop right here. No need to go down the rabbit hole.
if [ "${BASH_SOURCE[0]}" != "${0}" ]; then
    return
fi

set -e

export DEVICE=Mi8917
export DEVICE_COMMON=mithorium-common
export VENDOR=xiaomi

"./../../${VENDOR}/${DEVICE_COMMON}/extract-files.sh" "$@"
