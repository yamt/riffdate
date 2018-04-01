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
		LEN=$(echo -n ${B%%.*}|wc -c)
		if [ ${LEN} -gt 8 ]; then
			# XXX
			ORIG="${B##*-}"
			if [ "${F}" != "${ORIG}" ]; then
				echo "${F} => ${ORIG}"
			fi
		else
			ORIG="${B}"
		fi
		DIR="${LINKDIR}/${YEAR}/${DAY}"
		NEW="${TS}-${ORIG}"
		if [ -e "${DIR}/${NEW}" ]; then
			echo "skip $F (already exist)"
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
