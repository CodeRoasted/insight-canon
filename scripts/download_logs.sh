#!/bin/bash
# scripts/download_logs.sh
# Usage: bash download_logs.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

LOGHUB_DIR="$ROOT_DIR/data/logs/loghub"
ZENODO_DIR="$ROOT_DIR/data/logs/zenodo"

extract_zip() {
    local target="$1"

    if [[ -d "$target" ]]; then
        find "$target" -type f -name "*.zip" | while read -r zipfile; do
            extract_zip "$zipfile"
        done
        return
    fi

    if [[ "$target" == *.zip ]]; then
        echo "👉 Extracting: $target"

        local dir
        dir="$(dirname "$target")"

        if command -v python3 >/dev/null 2>&1; then
            python3 -m zipfile -e "$target" "$dir"
        elif command -v 7z >/dev/null 2>&1; then
            7z x -y -o"$dir" "$target"
        elif command -v unzip >/dev/null 2>&1; then
            unzip -o "$target" -d "$dir"
        elif command -v bsdtar >/dev/null 2>&1; then
            bsdtar -xf "$target" -C "$dir"
        else
            echo "❌ ERROR: no unzip tool found"
            exit 1
        fi

        rm -f "$target"
    fi
}


mkdir -p "$LOGHUB_DIR"
mkdir -p "$ZENODO_DIR"

########################################
# LOGHUB
########################################


echo "📦 Downloading Loghub 2k datasets..."

BASE_LOGHUB_URL="https://raw.githubusercontent.com/logpai/loghub/master"

# List of 2k datasets
LOGHUB_DATASETS=(
    "Android/Android_2k.log"
    "Apache/Apache_2k.log"
    "BGL/BGL_2k.log"
    "HDFS/HDFS_2k.log"
    "Hadoop/Hadoop_2k.log"
    "HealthApp/HealthApp_2k.log"
    "HPC/HPC_2k.log"
    "Linux/Linux_2k.log"
    "Mac/Mac_2k.log"
    "OpenSSH/OpenSSH_2k.log"
    "OpenStack/OpenStack_2k.log"
    "Proxifier/Proxifier_2k.log"
    "Spark/Spark_2k.log"
    "Thunderbird/Thunderbird_2k.log"
    "Windows/Windows_2k.log"
    "Zookeeper/Zookeeper_2k.log"
)

for dataset in "${LOGHUB_DATASETS[@]}"; do
    filename=$(basename "$dataset")
    echo "👉 $filename"
    curl -L -o "$LOGHUB_DIR/$filename" "$BASE_LOGHUB_URL/$dataset"
done

echo "✅ Loghub done"
echo ""

########################################
# ZENODO (ARCHIVE)
########################################

echo "📦 Downloading Zenodo archive..."

ZENODO_ARCHIVE_URL="https://zenodo.org/api/records/18522101/files-archive"
ARCHIVE_PATH="$ZENODO_DIR/zenodo_logs.zip"

curl -L --retry 5 --retry-delay 5 --retry-all-errors -o "$ARCHIVE_PATH" "$ZENODO_ARCHIVE_URL"

echo "👉 Extracting archive..."

# The archive contains multiple nested zip files, so we need to extract them recursively
extract_zip "$ZENODO_DIR"
extract_zip "$ZENODO_DIR"

rm -f "$ARCHIVE_PATH"

echo "✅ Zenodo done"

echo ""
echo "🎉 All datasets ready!"

########################################
# MIXED DATASETS
########################################

echo "🧪 Building mixed datasets..."

# 👉 RAW logs mix (Loghub)
echo "👉 Creating mixed_raw.log"
cat "$LOGHUB_DIR"/*.log > "$LOG_DIR/mixed_raw.log"

# 👉 JSON logs mix (Zenodo)
echo "👉 Creating mixed_structured.json"
find "$ZENODO_DIR" -type f -name "*.json" -exec cat {} + > "$LOG_DIR/mixed_structured.json"

echo "✅ Mixed datasets ready"
echo ""

echo "🎉 All datasets ready for Insight!"