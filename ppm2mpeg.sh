#!/bin/bash 

if ( test ${#} -lt 1 ) ; then
cat << !EOF
usage: $0 "%s" %d %d %d %d %f %d &
	%s: file name of the frames, e.g. 'cap-%04d.xwd'
	%d: number of the first saved frame
	%d: number of the last saved frame
	%d: width of the frame
	%d: height of the frame
	%f: frames per second
	%d: time per frame (ms, TPF = 1000 / FPS)

!EOF
exit
fi

LOC_TRANSCODE=`which transcode`
LOC_FFMPEG=`which ffmpeg`
LOC_MENCODER=`which mencoder`
EXTENSION=`echo "${1}" | nawk -F "." '{print tolower($NF)}'`
FILE=`echo "${1}" | sed 's/%[\.]*[0-9]*[i|d]/*/g'`
OUTPUTFILE="/tmp/output.avi"


#
# ffmpeg part
#

if ( test -z ${LOC_FFMPEG} ) ; then
	echo "ffmpeg not found, trying transcode ..."
else
	if test "x${EXTENSION}" = "xpng" -o "x${EXTENSION}" = "xppm" -o "x${EXTENSION}" = "xpgm" ; then
		echo "Encoding with ffmpeg found at: ${LOC_FFMPEG}"
		FFMPEG_FILE=`echo "${1}" | sed 's/%[\.]*/%/g'`

		${LOC_FFMPEG} -y -r ${6} -i ${FFMPEG_FILE} ${OUTPUTFILE}
		exit 0
	else
		echo "of xvidcap's output files ffmpeg only supports png, ppm, and pnm."
		echo "can't encode the input files you're providing, sorry."
		exit 1
	fi
fi


#
# transcode part
#

if ( test -z ${LOC_TRANSCODE} ) ; then
	echo "transcode not found, trying mencoder ..."
else
	echo "Encoding with transcode found at: ${LOC_TRANSCODE}"

	LIST="/tmp/xv_transcode_${$}.piclist"
	DIM="${4}x${5}"
	FPS="${6}"
	ls -1 ${FILE} | sort > ${LIST}
	${LOC_TRANSCODE} -i ${LIST} -g ${DIM} -x imlist,null -y ffmpeg,null -F mpeg4 -f ${FPS} -o ${OUTPUTFILE} -H 1 --use_rgb
	rm ${LIST}
	exit 0
fi


#
# mencoder part
#

if ( test -z ${LOC_MENCODER} ) ; then
	echo "mencoder not found either, sorry!"
else
	if test "x${EXTENSION}" = "xpng" -o "x${EXTENSION}" = "xjpg" -o x"${EXTENSION}" = "xjpeg" ; then
		echo "Encoding with mencoder found at: ${LOC_MENCODER}"

		${LOC_MENCODER} "mf://${FILE}" -mf fps=${6} -o ${OUTPUTFILE} -ovc lavc
	else
		echo "of xvidcap's output files mencoder only supports png and jpg."
		echo "can't encode the input files you're providing, sorry."
	fi
fi

exit
