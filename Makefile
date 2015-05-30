GCC = g++ 

INC_DIR = ./include
SRC_DIR = ./src
OBJ_DIR = ./obj

INC=-I/usr/include/irods/ -I/usr/local/include -I${INC_DIR} 
LIB=-L/usr/local/lib -lgdal -lboost_system -lboost_filesystem 

all: geometadata

geometadata:
	${GCC} ${INC} ${LIB} -fPIC -shared -o ${OBJ_DIR}/libmsiExtractGeoMeta.so ${SRC_DIR}/geometadata.cpp -Wno-deprecated -DRODS_SERVER -std=c++11 /usr/lib/irods/libirods_client.a

clean:
	@rm -f  ${OBJ_DIR}/*.so
