// =-=-=-=-=-=-=-
#include "apiHeaderAll.hpp"
#include "msParam.hpp"
#include "reGlobalsExtern.hpp"
#include "irods_ms_plugin.hpp"
#include "reAction.hpp"
#include "modAVUMetadata.hpp"
#include "dataObjOpr.hpp"

// =-=-=-=-=-=-=-
// STL Includes
#include <string>
#include <iostream>
#include <vector>


// =-=-=-=-=-=-=-
// GDAL Includes
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h> 
#include <cpl_conv.h>
#include <netcdf.h>

// =-=-=-=-=-=-=-
// Boost Includes
#include <boost/filesystem.hpp>


class geoMetadata {
private:
    // iRODS server handle
  ruleExecInfo_t *rei;
  char geoExt[5];
  int geoType;
  char objType[6];
  char objName[512];
  char filePath[512];
  msParam_t kvpairsparam;
  GDALDataset *poDataset;
  OGRDataSource *poDS;

  static const std::vector<std::string> rastertypes;
  static const std::vector<std::string> vectortypes;

  int geospatialType();

  void setGeoExtension();

  int shapefileComplete();

  int setMeta();

  void addMeta(char *key, char *value);

  void extractVectorBasicMeta();

  void extractVectorBounds();

  void extractRasterBasicMeta();

  void extractRasterBounds();

  void extractMetaShp();

  void extractMetaNetCDF();

  void extractMetaGeoTiff();
  

public:
  geoMetadata( ruleExecInfo_t *in_rei, char *logPath, char *phyPath ); 

  ~geoMetadata();

  int extractGeoMeta();

}; 	// class geoMetadata
