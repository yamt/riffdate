#! /bin/sh

# eg.
#	PIC="/disks/raid1/pic/"
#	DIR="114SIGMA"
#	cd ${PIC} && find DCIM/${DIR} -ctime -4 | ~/bin/mkdateidx3.sh"

set -e

LC_ALL=C
export LC_ALL
TZ=Asia/Tokyo
export TZ

LINKDIR=bydate3

RIFFDATE=~/bin/riffdate
EXIFPROBE=~/bin/exifprobe
FFPROBE=~/bin/ffprobe

if [ ! -x ${RIFFDATE} ]; then
	exit 1
fi
if [ ! -x ${EXIFPROBE} ]; then
	exit 1
fi

# XXX for OSX
# exifgrep seems using fgrep -n internally.
# OSX's fgrep doesn't have the option.
exifgrep()
{
	${EXIFPROBE} -L -- "$2" | egrep -- "$1"
}

parsedate()
{
	case "$(uname)" in
	Darwin)
		date -j -f '%Y-%m-%d %H:%M:%S' "$1" "$2";;
	*)
		date -d "$1" "$2";;
	esac
}

while read F; do
	DONE=0
	if [ ! -s ${F} ]; then
		echo "skip ${F} (zero sized)"
		continue
	fi
	case ${F} in
	*.JPG|*.jpg|*.ARW|*.arw)
		# XXX date -d doesn't work on OSX
		# date -j -f '%Y:%m:%d %H:%M:%S'

		# the output of the following exifgrep would be like the
		# following.  (OSX 10.8.2 pkgsrc exifprobe-2.0.1)
		# JPEG.APP1.Ifd0.Exif.DateTimeOriginal = '2012:09:21 04:18:42'
		# JS will be
		# 2012-09-21 04:18:42

                # ARW files seem to have:
                # TIFF.Ifd0.Exif.DateTimeOriginal
		JS=$(exifgrep "(TIFF|JPEG.APP1).Ifd0.Exif.DateTimeOriginal" "${F}" | awk '//{print $3 " " $4}'|sed -e 's/:/-/' -e 's/:/-/' -e "s/'//g")
		if [ -z "${JS}" ]; then
                        echo failed to extract timestamp from ${F}
			exit 2
		fi
		DAY=$(parsedate "${JS}" "+%Y%m%d")
		YEAR=$(parsedate "${JS}" "+%Y")
		TS=$(parsedate "${JS}" "+%Y%m%d-%H%M%S")
		DONE=1
		;;

	*.TIF|*.tif)
		# XXX date -d doesn't work on OSX
		# date -j -f '%Y:%m:%d %H:%M:%S'
		JS=$(exifgrep TIFF.Ifd0.Exif.DateTimeOriginal "${F}" | awk '//{print $3 " " $4}'|sed -e 's/:/-/' -e 's/:/-/' -e "s/'//g")
		if [ -z "${JS}" ]; then
			exit 2
		fi
		DAY=$(parsedate "${JS}" "+%Y%m%d")
		YEAR=$(parsedate "${JS}" "+%Y")
		TS=$(parsedate "${JS}" "+%Y%m%d-%H%M%S")
		DONE=1
		;;

	*.AVI|*.avi)
		# XXX date -d doesn't work on OSX
		# date -j -f '%Y:%m:%d %H:%M:%S'
		JS=$(${RIFFDATE} "${F}" | awk '$1~/^(ntcg-DateTimeOriginal|IDIT-exiftime):/ {print $2 " " $3}'|sed -e 's/:/-/' -e 's/:/-/' -e "s/'//g")
		if [ -z "${JS}" ]; then
			exit 2
		fi
		DAY=$(parsedate "${JS}" "+%Y%m%d")
		YEAR=$(parsedate "${JS}" "+%Y")
		TS=$(parsedate "${JS}" "+%Y%m%d-%H%M%S")
		DONE=1
		;;

	*.MP4|*.mp4)
		# The output of ffprobe looks like the following:
		#
		#     giraffe% ffprobe -i DSCN8038.MP4 2>&1
		#       built with Apple LLVM version 7.0.2 (clang-700.1.81)
		#       configuration: --prefix=/usr/local/Cellar/ffmpeg/3.2.4 --enable-shared --enable-pthreads --enable-gpl --enable-version3 --enable-hardcoded-tables --enable-avresample --cc=clang --host-cflags= --host-ldflags= --enable-libebur128 --enable-libfdk-aac --enable-libmp3lame --enable-libopus --enable-libx264 --enable-libxvid --enable-opencl --disable-lzma --enable-nonfree --enable-vda
		#       libavutil      55. 34.101 / 55. 34.101
		#       libavcodec     57. 64.101 / 57. 64.101
		#       libavformat    57. 56.101 / 57. 56.101
		#       libavdevice    57.  1.100 / 57.  1.100
		#       libavfilter     6. 65.100 /  6. 65.100
		#       libavresample   3.  1.  0 /  3.  1.  0
		#       libswscale      4.  2.100 /  4.  2.100
		#       libswresample   2.  3.100 /  2.  3.100
		#       libpostproc    54.  1.100 / 54.  1.100
		#     Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'DSCN8038.MP4':
		#       Metadata:
		#         major_brand     : mp42
		#         minor_version   : 1
		#         compatible_brands: mp42avc1niko
		#         creation_time   : 2018-04-01T21:24:53.000000Z
		#       Duration: 00:00:01.00, start: 0.000000, bitrate: 554 kb/s
		#         Stream #0:0(und): Video: h264 (High) (avc1 / 0x31637661), yuvj420p(pc, bt709), 1920x1080, 29 kb/s, 29.97 fps, 29.97 tbr, 30k tbn, 60k tbc (default)
		#         Metadata:
		#           creation_time   : 2018-04-01T21:24:53.000000Z
		#         Stream #0:1(und): Audio: aac (LC) (mp4a / 0x6134706D), 24000 Hz, stereo, fltp, 198 kb/s (default)
		#         Metadata:
		#           creation_time   : 2018-04-01T21:24:53.000000Z
		#     giraffe% 
		#
		# Note: Remove .000000Z suffix as it's actually a local time
		# it seems.  (At least for MP4 files generated by Nikon COOLPIX W100)
		# Note: Remove "T" to align with the parsedate's expectation.
		# Note: ffprobe outputs the info to stderr for some reasons.
		JS=$(${FFPROBE} -i "${F}" 2>&1 | sed -ne '/[[:space:]]*creation_time[[:space:]]*: *\([^.[:space:]]\)/{s//\1/;p;q;}' | sed -e 's/\.[0-9]*Z$//' | sed -e 's/T//')
		if [ -z "${JS}" ]; then
			exit 2
		fi
		DAY=$(parsedate "${JS}" "+%Y%m%d")
		YEAR=$(parsedate "${JS}" "+%Y")
		TS=$(parsedate "${JS}" "+%Y%m%d-%H%M%S")
		DONE=1
		;;

	*.X3F|*.x3f)
		J=$(exifgrep X3F.PROP.Data.TIME "${F}" | awk '//{print $3}')
		if [ -z "${J}" ]; then
			exit 2
		fi
		# Assumption: the timestamp is in JST (UTC-09)
		T="$((J - 9 * 60 * 60))"
		DAY=$(date -r ${T} "+%Y%m%d")
		YEAR=$(date -r ${T} "+%Y")
		TS=$(date -r ${T} "+%Y%m%d-%H%M%S")
		DONE=1
		;;
	esac
	if [ ${DONE} -eq 1 ]; then
		B=$(basename "${F}")
		LEN=$(printf "%s" ${B%%.*}|wc -c)
		if [ ${LEN} -gt 8 ]; then
			# XXX
			ORIG="${B##*-}"
			if [ "${F}" != "${ORIG}" ]; then
				echo "${F} (${ORIG})"
			fi
		else
			ORIG="${B}"
		fi
		DIR="${LINKDIR}/${YEAR}/${DAY}"
		NEW="${TS}-${ORIG}"
		if [ -e "${DIR}/${NEW}" ]; then
			echo "skip $F (${NEW} already exist)"
		else
			echo "mklink ${F} -> ${NEW} (${TS})"
			mkdir -p "${DIR}"
			ln "${F}" "${DIR}/${NEW}"
#			ln -s "../../${F}" "${DIR}/${NEW}"
		fi
	else
		echo "skip ${F} (unknown type)"
	fi
done
