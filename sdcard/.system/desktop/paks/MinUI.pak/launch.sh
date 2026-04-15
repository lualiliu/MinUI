#!/bin/sh

export PLATFORM="desktop"
export SDCARD_PATH="/mnt/sdcard"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export ROMS_PATH="$SDCARD_PATH/Roms"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

export PATH="$SYSTEM_PATH/bin:$PATH"
export LD_LIBRARY_PATH="$SYSTEM_PATH/lib:$LD_LIBRARY_PATH"

mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"
mkdir -p "$ROMS_PATH"

AUTO_PATH="$USERDATA_PATH/auto.sh"
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH"
fi

cd "$(dirname "$0")"

EXEC_PATH="/tmp/minui_exec"
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync

while [ -f "$EXEC_PATH" ]; do
	minui.elf > "$LOGS_PATH/minui.txt" 2>&1
	date +'%F %T' > "$DATETIME_PATH"
	sync

	if [ -f "$NEXT_PATH" ]; then
		sh "$NEXT_PATH"
		NEXT_RC=$?
		rm -f "$NEXT_PATH"
		printf '%s next rc=%s\n' "$(date +'%F %T')" "$NEXT_RC" >> "$LOGS_PATH/minui.txt"
		date +'%F %T' > "$DATETIME_PATH"
		sync
	fi

	# Avoid tight restart loops when a frontend/emulator exits immediately.
	sleep 0.1
done

