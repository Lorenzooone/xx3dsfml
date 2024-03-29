SYS := ${shell uname -s}
ARC := ${shell uname -m}
VER := 1.0.5
TAR := libftd3xx-linux-arm-v6-hf-${VER}.tgz
ZIP := echo "ZIP NOT SET"
FTD3XX_FOLDER := ftd3xx
SRC_FOLDER := ${FTD3XX_FOLDER}/downloads
CXX := g++
FRAMEWORKS_OPTIONS :=
CLEANUP_CMD := echo "DONE"
URL_TIME := 2023/03
ifeq (${SYS}, Darwin)
	EXT := dylib
	CXX := clang++
	FRAMEWORKS_OPTIONS := -framework Cocoa -framework OpenGL -framework IOKit
	LIB := libftd3xx-static.a
	VOL := d3xx-osx.${VER}
	SRC_FOLDER := /Volumes/${VOL}
	TAR := ${VOL}.dmg
	ZIP := hdiutil attach ${FTD3XX_FOLDER}/downloads/${TAR}
	CLEANUP_CMD := hdiutil detach ${SRC_FOLDER}
else ifeq (${SYS}, Linux)
	EXT := so
	LIB := libftd3xx-static.a
	ifeq (${ARC}, $(filter aarch% arm%, ${ARC}))
		ifeq (${ARC}, $(filter arm64% %64 armv8% %v8, ${ARC}))
			TAR := libftd3xx-linux-arm-v8-${VER}.tgz
		else ifeq (${ARC}, $(filter armv7% %v7, ${ARC}))
			TAR := libftd3xx-linux-arm-v7_32-${VER}.tgz
		endif
	else
		ifeq (${ARC}, $(filter %64, ${ARC}))
			TAR := libftd3xx-linux-x86_64-${VER}.tgz
		else ifeq (${ARC}, $(filter %32 %86, ${ARC}))
			TAR := libftd3xx-linux-x86_32-${VER}.tgz
		endif
	endif
	ZIP := tar -xzf ${FTD3XX_FOLDER}/downloads/${TAR} --strip-components 1 -C ${FTD3XX_FOLDER}/downloads
endif

xx3dsfml: xx3dsfml.o
	${CXX} xx3dsfml.o -o xx3dsfml -std=c++17 ${FRAMEWORKS_OPTIONS} ${FTD3XX_FOLDER}/libftd3xx-static.a -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window -lpthread

xx3dsfml.o: xx3dsfml.cpp
	${CXX} -std=c++17 -I ${FTD3XX_FOLDER} -c xx3dsfml.cpp -o xx3dsfml.o

clean:
	rm -f xx3dsfml *.o

install: uninstall clean xx3dsfml
	sudo mkdir -p /usr/local/bin
	sudo mv xx3dsfml /usr/local/bin

uninstall: uninstall_ftd3xx
	sudo rm -f /usr/local/bin/xx3dsfml

download_ftd3xx: remove_ftd3xx
	mkdir -p ${FTD3XX_FOLDER}/downloads
	curl https://ftdichip.com/wp-content/uploads/${URL_TIME}/${TAR} -o ${FTD3XX_FOLDER}/downloads/${TAR}
	${ZIP}
	cp ${SRC_FOLDER}/${LIB} ${FTD3XX_FOLDER}
	cp ${SRC_FOLDER}/*.h ${FTD3XX_FOLDER}
	rm -fr ${FTD3XX_FOLDER}/downloads
	${CLEANUP_CMD}

remove_ftd3xx:
	rm -fr ${FTD3XX_FOLDER}
