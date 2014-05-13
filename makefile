CXX_FLAGS   = -std=c++11
LD_FLAGS    = -lglfw3 -framework opengl
OUT_DIR     = bin
OUT_FILE    = ogl_test
INCLUDES    = -Isrc -Iglm

SRC_FILES   = \
	src/main.cpp src/picopng.cpp \
	src/gl/util.cpp src/mesh/mesh.cpp

${OUT_DIR}/${OUT_FILE}: ${SRC_FILES}
	g++ ${SRC_FILES} -o ${OUT_DIR}/${OUT_FILE} ${INCLUDES} ${CXX_FLAGS} ${LD_FLAGS}

run: ${OUT_DIR}/${OUT_FILE}
	${OUT_DIR}/${OUT_FILE}

default: ${OUT_DIR}/${OUT_FILE}

clean:
	rm -rf bin/*
