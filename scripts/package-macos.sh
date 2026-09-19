#!/usr/bin/env bash
#
# Build, sign and package the macOS development/release bundles.
#
#   scripts/package-macos.sh                 build, sign and make a DMG
#   scripts/package-macos.sh --no-build      sign existing artefacts
#   scripts/package-macos.sh --notarize      notarize and staple bundles + DMG
#
# The default CMake post-build signature is ad-hoc for local development. This
# script replaces it with a Developer ID Application signature before packaging.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

VERSION="$(tr -d ' \n' < VERSION)"
PRODUCT="Beat Volumetric"
CONFIG="${CONFIG:-release}"
BUILD_DIR="build/${CONFIG}"
ARTEFACT_CONFIG="Release"
[ "$CONFIG" = "debug" ] && ARTEFACT_CONFIG="Debug"
ARTEFACTS="${BUILD_DIR}/src/plugin/BeatVolumetric_artefacts/${ARTEFACT_CONFIG}"
ENTITLEMENTS="packaging/macos/BeatVolumetric.entitlements"
STAGING="${BUILD_DIR}/dmg-staging"
DIST="dist"
DMG="${DIST}/${PRODUCT} ${VERSION}.dmg"

APP="${ARTEFACTS}/Standalone/BeatVolumetricDev.app"
AU="${ARTEFACTS}/AU/BeatVolumetricDev.component"
VST3="${ARTEFACTS}/VST3/BeatVolumetricDev.vst3"

BUILD=1
NOTARIZE=0
IDENTITY="${CODESIGN_IDENTITY:-}"
PROFILE="${NOTARY_PROFILE:-beat-volumetric}"

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) BUILD=0 ;;
        --notarize) NOTARIZE=1 ;;
        --identity)
            [ $# -ge 2 ] || { echo "--identity requires a value" >&2; exit 2; }
            IDENTITY="$2"
            shift
            ;;
        --keychain-profile)
            [ $# -ge 2 ] || { echo "--keychain-profile requires a value" >&2; exit 2; }
            PROFILE="$2"
            shift
            ;;
        -h|--help)
            sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            exit 2
            ;;
    esac
    shift
done

[ "$(uname -s)" = "Darwin" ] || { echo "this script requires macOS" >&2; exit 1; }

say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

# Resolve a Developer ID identity to its SHA-1 fingerprint. Signing by
# fingerprint avoids the "ambiguous" error when the same certificate is in
# both the login and System keychains.
IDENTITY_NAME=""
if [ -z "$IDENTITY" ]; then
    IDENTITY="$(security find-identity -v -p codesigning \
        | awk '/Developer ID Application/{print $2; exit}')"
    IDENTITY_NAME="$(security find-identity -v -p codesigning \
        | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -1)"
elif printf '%s' "$IDENTITY" | grep -Eq '^[0-9A-Fa-f]{40}$'; then
    IDENTITY_NAME="$(security find-identity -v -p codesigning \
        | awk -v fingerprint="$IDENTITY" '$2 == fingerprint {
            sub(/^[[:space:]]*[0-9]+\)[[:space:]]*[0-9A-Fa-f]+[[:space:]]*"/, "");
            sub(/"[[:space:]]*$/, ""); print; exit
        }')"
else
    IDENTITY_NAME="$IDENTITY"
    IDENTITY="$(security find-identity -v -p codesigning \
        | awk -v name="$IDENTITY_NAME" 'index($0, "\"" name "\"") {
            print $2; exit
        }')"
fi

if [ -z "$IDENTITY" ] || [ -z "$IDENTITY_NAME" ] \
    || ! printf '%s' "$IDENTITY_NAME" | grep -q '^Developer ID Application:'; then
    cat >&2 <<'MSG'
No Developer ID Application identity was found in the Keychain.

The package must be signed with Developer ID Application, not Apple Development
or an ad-hoc identity. Check:

    security find-identity -v -p codesigning

If the certificate is present but the identity is missing, its private key is
probably on another Mac. Export/import the certificate together with its .p12
private key, or create a new Developer ID Application certificate here.
MSG
    exit 1
fi

TEAM_ID="$(printf '%s\n' "$IDENTITY_NAME" | sed -n 's/.*(\([A-Z0-9][A-Z0-9]*\))$/\1/p')"
[ -n "$TEAM_ID" ] || { echo "could not derive Team ID from: $IDENTITY_NAME" >&2; exit 1; }

say "signing identity"
echo "  $IDENTITY_NAME"
echo "  fingerprint: $IDENTITY"
echo "  team: $TEAM_ID"

if [ "$BUILD" -eq 1 ]; then
    say "build $CONFIG"
    cmake --preset "$CONFIG" > /dev/null
    cmake --build --preset "$CONFIG" --target \
        BeatVolumetric_VST3 BeatVolumetric_AU BeatVolumetric_Standalone
fi

for bundle in "$APP" "$AU" "$VST3"; do
    [ -d "$bundle" ] || { echo "missing build artefact: $bundle" >&2; exit 1; }
done

sign_bundle()
{
    local bundle="$1"

    # Sign nested code before its containing bundle. Do not use --deep for
    # signing: it can apply unsuitable signing options to nested code.
    while IFS= read -r -d '' nested; do
        codesign --force --options runtime --timestamp \
            --sign "$IDENTITY" "$nested"
    done < <(find "$bundle" \( -name '*.dylib' -o -name '*.framework' \) -print0 2>/dev/null)

    # The standalone opens an audio input device. AU/VST3 plug-ins inherit
    # audio permissions from their host and do not declare microphone access.
    if [ "$bundle" = "$APP" ]; then
        codesign --force --options runtime --timestamp \
            --entitlements "$ENTITLEMENTS" --sign "$IDENTITY" "$bundle"
    else
        codesign --force --options runtime --timestamp \
            --sign "$IDENTITY" "$bundle"
    fi
    codesign --verify --strict --verbose=2 "$bundle" 2>&1 | sed 's/^/    /'
}

say "sign bundles"
for bundle in "$AU" "$VST3" "$APP"; do
    echo "  $(basename "$bundle")"
    sign_bundle "$bundle"
done

notarize()
{
    local archive="$1"
    say "notarize $(basename "$archive")"
    xcrun notarytool submit "$archive" --keychain-profile "$PROFILE" --wait
}

if [ "$NOTARIZE" -eq 1 ]; then
    # ZIP cannot be stapled directly. Notarize one archive, then staple the
    # ticket to each bundle and create the final DMG from those stapled bundles.
    BATCH_DIR="${BUILD_DIR}/notarize"
    BATCH="${BUILD_DIR}/notarize.zip"
    rm -rf "$BATCH_DIR" "$BATCH"
    mkdir -p "$BATCH_DIR"
    cp -R "$APP" "$AU" "$VST3" "$BATCH_DIR/"
    ditto -c -k --keepParent --sequesterRsrc "$BATCH_DIR" "$BATCH"

    notarize "$BATCH"
    for bundle in "$APP" "$AU" "$VST3"; do
        xcrun stapler staple "$bundle"
        xcrun stapler validate "$bundle"
    done
    rm -rf "$BATCH_DIR" "$BATCH"
fi

say "make DMG"
rm -rf "$STAGING"
mkdir -p "$STAGING/Plug-Ins" "$DIST"
cp -R "$APP" "$STAGING/"
cp -R "$AU" "$STAGING/Plug-Ins/"
cp -R "$VST3" "$STAGING/Plug-Ins/"
ln -s /Applications "$STAGING/Applications"
rm -f "$DMG"
hdiutil create -volname "$PRODUCT $VERSION" -srcfolder "$STAGING" \
    -ov -format ULFO "$DMG" > /dev/null
codesign --force --timestamp --sign "$IDENTITY" "$DMG"

if [ "$NOTARIZE" -eq 1 ]; then
    notarize "$DMG"
    xcrun stapler staple "$DMG"
    xcrun stapler validate "$DMG"
fi

say "verify"
for bundle in "$APP" "$AU" "$VST3"; do
    echo "  $(basename "$bundle")"
    codesign --verify --deep --strict --verbose=2 "$bundle" 2>&1 | sed 's/^/    /'
done
echo "  Gatekeeper app:"
spctl --assess --type exec --verbose=4 "$APP" 2>&1 | sed 's/^/    /' || true
echo "  Gatekeeper DMG:"
spctl --assess --type open --context context:primary-signature --verbose=2 "$DMG" 2>&1 \
    | sed 's/^/    /' || true

if [ "$NOTARIZE" -eq 0 ]; then
    cat <<'MSG'

Notarization was skipped. The bundle is signed for local testing, but Gatekeeper
may reject it on another Mac. Use --notarize for external distribution.
MSG
fi

say "ready: $DMG"
du -h "$DMG" | sed 's/^/    /'
