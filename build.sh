
ROOT_DIR=$(dirname "$0")
BUILD_DIR="${ROOT_DIR}/build"
SRC_DIR="${ROOT_DIR}/src"


if [[ "$CC" == "" ]]; then
	CC="gcc"
fi

NCURSES_HEADER_DEF=""

if [[ -f /usr/include/ncurses.h ]]; then
	NCURSES_HEADER_DEF="HAVE_NCURSES_H"
elif [[ -f /usr/include/ncursesw/ncursesw.h ]]; then
	NCURSES_HEADER_DEF="HAVE_NCURSESW_NCURSESW_H"
elif [[ -f /usr/include/ncursesw.h ]]; then
	NCURSES_HEADER_DEF="HAVE_NCURSESW_H"
else
	echo "Please install ncurses..."
	exit 0
fi

CFLAGS="-std=c11 -Wall -Wextra -DNCURSES_WIDECHAR -D${NCURSES_HEADER_DEF}"
CFLAGS_DEBUG="${CFLAGS} -DDEBUG_ -O0 -g -fsanitize=address"
CFLAGS_RELEASE="${CFLAGS} -O3"
LIBS="-lminiupnpc -lncursesw"

if [[ "$1" == "release" ]]; then
	CFLAGS=$CFLAGS_RELEASE
else
	CFLAGS=$CFLAGS_DEBUG
fi

set -x
mkdir -p $BUILD_DIR
$CC $CFLAGS $SRC_DIR/*.c -o $BUILD_DIR/ltmsg $LIBS

