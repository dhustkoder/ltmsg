
ROOT_DIR=$(dirname "$0")
BUILD_DIR="${ROOT_DIR}/build"
SRC_DIR="${ROOT_DIR}/src"

if [[ "$CC" == "" ]]; then
	CC="gcc"
fi

CFLAGS="-std=c11 -Wall -Wextra -DNCURSES_WIDECHAR"
CFLAGS_DEBUG="${CFLAGS} -DDEBUG_ -O0 -g -fsanitize=address"
CFLAGS_RELEASE="${CFLAGS} -O3"
LIBS="-lminiupnpc -lncurses"

if [[ "$1" == "release" ]]; then
	CFLAGS=$CFLAGS_RELEASE
else
	CFLAGS=$CFLAGS_DEBUG
fi

set -x
mkdir -p $BUILD_DIR
$CC $CFLAGS $LIBS $SRC_DIR/*.c -o $BUILD_DIR/ltmsg

