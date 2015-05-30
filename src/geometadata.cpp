#include "geometadata.hpp"

geoMetadata::geoMetadata( ruleExecInfo_t *in_rei, char *logPath, char *phyPath ) {
  rei = in_rei;
  
  //-d since we are setting metadata for a file
  snprintf(objType, sizeof objType, "-d");
  
  //objPath is the logical path to the file 
  snprintf(objName, sizeof objName, "%s",logPath);
  
  //filePath is the physical path to the file 
  snprintf(filePath, sizeof filePath, "%s",phyPath);
  
  //initialize key value pairs object
  //pair to this struct
  (&kvpairsparam)->type = strdup(KeyValPair_MS_T);
  (&kvpairsparam)->inOutStruct = NULL;

  //set extension field to geospatial file's extension
  setGeoExtension();

  //set type code based on geospatial file's type
  //1 = raster, 2 = vector
  geoType = geospatialType();

  //initialize the dataset corresponding to geospatial file type
  if(geoType == 1) //raster
    {
      //register all raster format drivers
      GDALAllRegister();
      
      poDataset = (GDALDataset *) GDALOpen( filePath, GA_ReadOnly );
    }
  else if (geoType == 2)  //vector
    {
      //register all vector format drivers
      OGRRegisterAll();
      
      //we need to make sure that the bare minimum of related files are present
      //if so, modify objName and filePath to point to shapefile instead
      if(shapefileComplete())
	{
	  poDS = (OGRDataSource *) OGRSFDriverRegistrar::Open ( filePath, FALSE);
	}
      else
	{
	  poDS = NULL;
	}
    }
  
}

geoMetadata::~geoMetadata() {

}

void geoMetadata::setGeoExtension() {

  msParam_t msLogPath, parentPath, childName;
  char logPath[512], fileName[256];

  snprintf(logPath, sizeof logPath, "%s", objName);
  fillStrInMsParam(&msLogPath,logPath);
  
  msiSplitPath(&msLogPath,&parentPath,&childName,rei);
  
  snprintf(fileName, sizeof fileName, "%s", ((char *)childName.inOutStruct));

  snprintf(geoExt, sizeof geoExt, "%s", boost::filesystem::path(fileName).extension().c_str());

  return;

}

int geoMetadata::geospatialType() {

  int vectorp, rasterp, type;

  rasterp = std::find(std::begin(rastertypes),std::end(rastertypes),geoExt) != std::end(rastertypes);
  vectorp = std::find(std::begin(vectortypes),std::end(vectortypes),geoExt) != std::end(vectortypes);

  type = rasterp ? 1 : vectorp ? 2 : 0;

  return type;
  
}

int geoMetadata::shapefileComplete()
{
  
  msParam_t msLogPath, msPhyPath, parentPath, childName;
  char logPath[512], phyPath[512];
  char fileName[256];
  
  snprintf(logPath, sizeof logPath, "%s", objName);
  snprintf(phyPath, sizeof phyPath, "%s", filePath);
  fillStrInMsParam(&msLogPath,logPath);
  fillStrInMsParam(&msPhyPath,phyPath);
  
  msiSplitPath(&msLogPath,&parentPath,&childName,rei);
  
  
  snprintf(logPath, sizeof logPath, "%s", ((char *)parentPath.inOutStruct));
  snprintf(fileName, sizeof fileName, "%s", ((char *)childName.inOutStruct));
  
  msiSplitPath(&msPhyPath,&parentPath,&childName,rei);
  
  snprintf(phyPath, sizeof phyPath, "%s", ((char *)parentPath.inOutStruct));
  
  char *baseName;
  
  const char delim[2] = ".";
  
  //find common prefix
  baseName = strtok(fileName,delim);
  
  char path_dbf[512],path_shp[512],path_shx[512],path_prj[512];
  
  FILE *dbfp, *prjp, *shpp, *shxp;
  
  //construct paths to necessary files to check if present
  snprintf(path_prj, sizeof path_prj, "%s/%s.prj", phyPath, baseName);
  snprintf(path_dbf, sizeof path_dbf, "%s/%s.dbf", phyPath, baseName);
  snprintf(path_shp, sizeof path_shp, "%s/%s.shp", phyPath, baseName);
  snprintf(path_shx, sizeof path_shx, "%s/%s.shx", phyPath, baseName);
  
  if(boost::filesystem::exists(path_prj) && 
     boost::filesystem::exists(path_dbf) && 
     boost::filesystem::exists(path_shx) && 
     boost::filesystem::exists(path_shp))
    {
      snprintf(objName, sizeof path_shp, "%s/%s.shp", logPath, baseName);
      snprintf(filePath, sizeof path_shp, "%s/%s.shp", phyPath, baseName);
      return 1;
    }
  else
    {
      return 0;
    }

}

int geoMetadata::setMeta()
{
  msParam_t objnameparam, objtypeparam;
  int status;
  
  //set the obj type and name parameters
  fillStrInMsParam(&objnameparam, objName);
  fillStrInMsParam(&objtypeparam, objType);
  
  //add all the metadata field-name pairs to the file
  status = msiSetKeyValuePairsToObj(&kvpairsparam,&objnameparam,&objtypeparam,rei);
  
  return status;
  
}

void geoMetadata::addMeta(char *key, char *value)
{
  //MSParam struct objects to store metadata field name and value
  msParam_t keyparam, valparam;
  fillStrInMsParam(&keyparam,key);
  fillStrInMsParam(&valparam,value);
  msiAddKeyVal(&kvpairsparam,&keyparam,&valparam,rei);
}

void geoMetadata::extractVectorBasicMeta() {

  char metaname[128];
  char metavalue[128];
  
  OGRSFDriver *hDriver = poDS->GetDriver();
  
  //extract vector format 
  snprintf(metaname, sizeof metaname, "format");
  snprintf(metavalue, sizeof metavalue, "%s", hDriver->GetName());
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "type");
  snprintf(metavalue, sizeof metavalue, "geospatial");
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "language");
  snprintf(metavalue, sizeof metavalue, "%s", hDriver->GetName());
  addMeta(metaname, metavalue);
  
  extractVectorBounds();
  
  return;
  
}

void geoMetadata::extractVectorBounds() {

  char metaname[128];
  char metavalue[128];
  
  OGRSpatialReference *hSpatialRef;
  OGREnvelope *hExtent = new OGREnvelope();
  OGRCoordinateTransformation *poCT;
  
  //desired spatial reference
  OGRSpatialReference *targetSpatialRef = new OGRSpatialReference();
  targetSpatialRef->importFromEPSG(4326);
  
  //get the first layer
  OGRLayer *hLayer = poDS->GetLayer(0);
  
  //get layer's projection and extents
  hSpatialRef = hLayer->GetSpatialRef();
  
  snprintf(metaname, sizeof metaname, "projection");
  snprintf(metavalue, sizeof metavalue, "%s", (char *)hSpatialRef->GetAttrValue("PROJECTION",0));
  addMeta(metaname,metavalue);
  
  hLayer->GetExtent(hExtent);
  
  poCT = OGRCreateCoordinateTransformation(hSpatialRef,targetSpatialRef) ;
  
  double x1,y1,x2,y2,x11,y11,x21,y21;
  int transformed = 0;
  
  x1 = hExtent->MinX;
  y1 = hExtent->MinY;
  x2 = hExtent->MaxX;
  y2 = hExtent->MaxY;
  
  x11 = x1;
  x21 = x2;
  y11 = y1;
  y21 = y2;
  
  //extract natural and re-projected extents
  snprintf(metaname, sizeof metaname, "northlimit");
  snprintf(metavalue, sizeof metavalue, "%f", y2);
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "eastlimit");
  snprintf(metavalue, sizeof metavalue, "%f", x2);
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "westlimit");
  snprintf(metavalue, sizeof metavalue, "%f", x1);
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "southlimit");
  snprintf(metavalue, sizeof metavalue, "%f", y1);
  addMeta(metaname, metavalue);
  
  if(NULL != poCT)
    {
      if(poCT->Transform(1,&x11,&y11))
	{
	  if (poCT->Transform(1,&x21,&y21))
	    {
	      transformed = 1;
	      snprintf(metaname, sizeof metaname, "latmax");
	      snprintf(metavalue, sizeof metavalue, "%f", y21);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "lonmax");
	      snprintf(metavalue, sizeof metavalue, "%f", x21);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "lonmin");
	      snprintf(metavalue, sizeof metavalue, "%f", x11);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "latmin");
	      snprintf(metavalue, sizeof metavalue, "%f", y11);
	      addMeta(metaname, metavalue);
	    }
	}
    }
  
  if(!transformed)
    {
      snprintf(metaname, sizeof metaname, "latmax");
      snprintf(metavalue, sizeof metavalue, "%f", y2);
      addMeta(metaname, metavalue);
      
      snprintf(metaname, sizeof metaname, "lonmax");
      snprintf(metavalue, sizeof metavalue, "%f", x2);
      addMeta(metaname, metavalue);
      
      snprintf(metaname, sizeof metaname, "lonmin");
      snprintf(metavalue, sizeof metavalue, "%f", x1);
      addMeta(metaname, metavalue);
      
      snprintf(metaname, sizeof metaname, "latmin");
      snprintf(metavalue, sizeof metavalue, "%f", y1);
      addMeta(metaname, metavalue);
    }
  
  return;
  
}

void geoMetadata::extractMetaShp()
{
  //if the dataset object is NULL just return
  //constructor initializes it and may fail to do so
  //if the shapefile dataset is incompletely uploaded
  //requires a shx, shp, prj and dbf file atleast
  if( poDS == NULL )
    {
      rodsLog(LOG_ERROR, "Error occurred during shapefile metadata extraction: null dataset. Make sure atleast the .prj, .shp, .shx and .dbf files are present.");
      rei->status = -1;
      return;
    }
  
  extractVectorBasicMeta();
  
  char subject[2700];
  char metaname[128];
  
  //get the first layer
  OGRLayer *hLayer = poDS->GetLayer(0);
  
  //get the definition of a feature from the layer
  OGRFeatureDefn *hFDefn = hLayer->GetLayerDefn();
  int iField;
  
  //each feature has attribute fields
  //extract the names of these attributes to be used as metadata
  //for search
  for( iField = 0; iField < hFDefn->GetFieldCount(); iField++ )
    {
      OGRFieldDefn *hFieldDefn = hFDefn->GetFieldDefn( iField );
      
      if(iField == 0)
	{
	  strcpy(subject,hFieldDefn->GetNameRef());
	}
      else
	{
	  strcat(subject,",");
	  strcat(subject,hFieldDefn->GetNameRef());
	}
    }
  
  snprintf(metaname, sizeof metaname, "subject");
  addMeta(metaname, subject);
  
  rei->status = setMeta();
  
  return;
}

void geoMetadata::extractRasterBasicMeta() {

  char metaname[128];
  char metavalue[128];
  
  GDALDriver *hDriver = poDataset->GetDriver();
  
  //extract raster format 
  snprintf(metaname, sizeof metaname, "format");
  snprintf(metavalue, sizeof metavalue, "%s", hDriver->GetMetadataItem(GDAL_DMD_EXTENSION));
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "type");
  snprintf(metavalue, sizeof metavalue, "geospatial");
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "language");
  snprintf(metavalue, sizeof metavalue, "%s", hDriver->GetMetadataItem(GDAL_DMD_EXTENSION));
  addMeta(metaname, metavalue);
  
  extractRasterBounds();
  
  return;
  
}

void geoMetadata::extractRasterBounds() {

  char metaname[128];
  char metavalue[128];
  
  int xsize = poDataset->GetRasterXSize();
  int ysize = poDataset->GetRasterYSize();
  
  snprintf(metaname, sizeof metaname, "xsize");
  snprintf(metavalue, sizeof metavalue, "%d",xsize);
  addMeta(metaname, metavalue);
  
  snprintf(metaname, sizeof metaname, "ysize");
  snprintf(metavalue, sizeof metavalue, "%d",ysize);
  addMeta(metaname, metavalue);
  
  //set coverage information
  //includes north,east,west and southlimit and projection if any
  //will also store bounds in lat-lon to make it easier to search
  if( poDataset->GetProjectionRef() != NULL )
    {
      OGRSpatialReference *hSpatialRef, *targetSpatialRef;
      char *pszProjection;
      double adfGeoTransform[6];
      
      pszProjection = (char *)poDataset->GetProjectionRef();
      hSpatialRef = new OGRSpatialReference( pszProjection );
      targetSpatialRef = hSpatialRef->CloneGeogCS();
      
      OGRCoordinateTransformation *hTransform = OGRCreateCoordinateTransformation(hSpatialRef,targetSpatialRef);
      
      if( poDataset->GetGeoTransform( adfGeoTransform ) == CE_None )
	{ //is georeferenced
	  double dfGeoX,dfGeoY,northlimit,eastlimit,westlimit,southlimit,northlimit_latlon,eastlimit_latlon,westlimit_latlon,southlimit_latlon;
	  
	  dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * 0.0 + adfGeoTransform[2] * 0.0;
	  dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * 0.0 + adfGeoTransform[5] * 0.0;
	  
	  northlimit = dfGeoY;
	  westlimit = dfGeoX;
	  
	  if(NULL != hTransform)
	    {
	      hTransform->Transform(1,&dfGeoX,&dfGeoY);
	      northlimit_latlon = dfGeoY;
	      westlimit_latlon = dfGeoX;
	    }
	  
	  dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * 0.0 + adfGeoTransform[2] * ysize;
	  dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * 0.0 + adfGeoTransform[5] * ysize;
	  
	  if(dfGeoX < westlimit)
	    westlimit = dfGeoX;
	  southlimit = dfGeoY;
	  
	  if(NULL != hTransform)
	    {
	      hTransform->Transform(1,&dfGeoX,&dfGeoY);
	      if(dfGeoX < westlimit_latlon)
		westlimit_latlon = dfGeoX;
	      southlimit_latlon = dfGeoY;
	    }
	  
	  dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * xsize + adfGeoTransform[2] * 0.0;
	  dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * xsize + adfGeoTransform[5] * 0.0;
	  
	  eastlimit = dfGeoX;
	  if(dfGeoY > northlimit)
	    northlimit = dfGeoY;
	  
	  if(NULL != hTransform)
	    {
	      hTransform->Transform(1,&dfGeoX,&dfGeoY);
	      eastlimit_latlon = dfGeoX;
	      if(dfGeoY > northlimit_latlon)
		northlimit_latlon = dfGeoY;
	    }
	  
	  dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * xsize + adfGeoTransform[2] * ysize;
	  dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * xsize + adfGeoTransform[5] * ysize;
	  
	  if(dfGeoX > eastlimit)
	    eastlimit = dfGeoX;
	  if(dfGeoY < southlimit)
	    southlimit = dfGeoY;
	  
	  if(NULL != hTransform)
	    {
	      hTransform->Transform(1,&dfGeoX,&dfGeoY);
	      if(dfGeoX > eastlimit_latlon)
		eastlimit_latlon = dfGeoX;
	      if(dfGeoY < southlimit_latlon)
		southlimit_latlon = dfGeoY;
	    }
	  
	  //projection attribute for coverage
	  snprintf(metaname, sizeof metaname, "projection");
	  snprintf(metavalue, sizeof metavalue, "%s", hSpatialRef->GetAttrValue("PROJECTION",0));
	  addMeta(metaname, metavalue);
	  
	  snprintf(metaname, sizeof metaname, "northlimit");
	  snprintf(metavalue, sizeof metavalue, "%f", northlimit);
	  addMeta(metaname, metavalue);
	  
	  snprintf(metaname, sizeof metaname, "eastlimit");
	  snprintf(metavalue, sizeof metavalue, "%f", eastlimit);
	  addMeta(metaname, metavalue);
	  
	  snprintf(metaname, sizeof metaname, "westlimit");
	  snprintf(metavalue, sizeof metavalue, "%f", westlimit);
	  addMeta(metaname, metavalue);
	  
	  snprintf(metaname, sizeof metaname, "southlimit");
	  snprintf(metavalue, sizeof metavalue, "%f", southlimit);
	  addMeta(metaname, metavalue);
	  
	  if(NULL != hTransform)
	    {
	      snprintf(metaname, sizeof metaname, "latmax");
	      snprintf(metavalue, sizeof metavalue, "%f", northlimit_latlon);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "lonmax");
	      snprintf(metavalue, sizeof metavalue, "%f", eastlimit_latlon);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "lonmin");
	      snprintf(metavalue, sizeof metavalue, "%f", westlimit_latlon);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "latmin");
	      snprintf(metavalue, sizeof metavalue, "%f", southlimit_latlon);
	      addMeta(metaname, metavalue);
	      
	    }
	  else
	    {
	      snprintf(metaname, sizeof metaname, "latmax");
	      snprintf(metavalue, sizeof metavalue, "%f", northlimit);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "lonmax");
	      snprintf(metavalue, sizeof metavalue, "%f", eastlimit);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "lonmin");
	      snprintf(metavalue, sizeof metavalue, "%f", westlimit);
	      addMeta(metaname, metavalue);
	      
	      snprintf(metaname, sizeof metaname, "latmin");
	      snprintf(metavalue, sizeof metavalue, "%f", southlimit);
	      addMeta(metaname, metavalue);
	      
	    }
	  
	}
    }
}

/*the extraction of description, subject & title will differ
  based on whether the file is netcdf or geotiff. For geotiff
  we simply look for these fields in the metadata and in subdatasets
  for netcdf, we use the netcdf c APIs to find the global and variable-specific
  attribute values*/

void geoMetadata::extractMetaGeoTiff()
{
  if( poDataset == NULL )
    {
      rodsLog(LOG_ERROR, "Error occurred during geotiff metadata extraction : null dataset");
      rei->status = -1;
      return;
    }
  
  extractRasterBasicMeta();
  
  GDALDriver *hDriver = poDataset->GetDriver();
  modAVUMetadataInp_t modAVUMetadataInp;
  
  char **geoMetadata = hDriver->GetMetadata( NULL );
  int i,j;
  char metaname[128];
  char metavalue[512];
  
  if(CSLCount(geoMetadata) > 0 )
    {
      for( i = 0; geoMetadata[i] != NULL; i++ )
	{
	  char **name = hDriver->GetMetadata( NULL);
	  char *value = (char *)CPLParseNameValue(geoMetadata[i],name);
	  if((strcasecmp(*name,"Description") == 0) || (strcasecmp(*name,"Title") == 0))
	    {
	      snprintf(metaname, sizeof metaname, "description");
	      snprintf(metavalue, sizeof metavalue,"%s", value);
	      addMeta (metaname, metavalue);
	      snprintf(metaname, sizeof metaname, "title");
	      addMeta (metaname, metavalue);
	      snprintf(metaname, sizeof metaname, "subject");
	      addMeta (metaname, metavalue);
	    }
	  if(strcasecmp(*name,"History") == 0)
	    {
	      snprintf(metaname, sizeof metaname, "source");
	      snprintf(metavalue, sizeof metavalue,"%s", value);
	      addMeta (metaname, value);
	    }
	}
    }
  
  //Call geoMetadata::setMeta to set previously extracted metadata to 
  //the iRODS file
  
  rei->status  = setMeta();
  
  char **papszSubdatasets = hDriver->GetMetadata( "SUBDATASETS" );
  int nSubdatasets = CSLCount( papszSubdatasets );
  
  if ( nSubdatasets > 0)
    {
      char szKeyName[1024];
      char *pszSubdatasetName;
      char *pszSubdatasetDesc;
      char op[10];
      
      snprintf(op, sizeof op, "add");
      
      for (i = 1; i <= nSubdatasets/2; i++)
	{
	  
	  snprintf( szKeyName, sizeof(szKeyName),
		    "SUBDATASET_%d_NAME", i );
	  szKeyName[sizeof(szKeyName) - 1] = '\0';
	  pszSubdatasetName =
	    (char *) CSLFetchNameValue( papszSubdatasets, szKeyName );
	  snprintf( szKeyName, sizeof(szKeyName),
		    "SUBDATASET_%d_DESC", i );
	  szKeyName[sizeof(szKeyName) - 1] = '\0';
	  pszSubdatasetDesc =
	    (char *) CSLFetchNameValue( papszSubdatasets, szKeyName );
	  
	  
	  bzero (&modAVUMetadataInp, sizeof (modAVUMetadataInp));
	  modAVUMetadataInp.arg0 = op;
	  modAVUMetadataInp.arg1 = objType;
	  modAVUMetadataInp.arg2 = objName;
	  snprintf(metaname, sizeof metaname, "description");
	  modAVUMetadataInp.arg3 = metaname;
	  modAVUMetadataInp.arg4 = pszSubdatasetDesc;
	  modAVUMetadataInp.arg5 = pszSubdatasetName;
	  rsModAVUMetadata(rei->rsComm, &modAVUMetadataInp);
	  
	  snprintf(metaname, sizeof metaname, "title");
	  modAVUMetadataInp.arg3 = metaname;
	  modAVUMetadataInp.arg4 = pszSubdatasetDesc;
	  modAVUMetadataInp.arg5 = pszSubdatasetName;
	  rsModAVUMetadata(rei->rsComm, &modAVUMetadataInp);
	  
	  snprintf(metaname, sizeof metaname, "subject");
	  modAVUMetadataInp.arg3 = metaname;
	  modAVUMetadataInp.arg4 = pszSubdatasetName;
	  modAVUMetadataInp.arg5 = pszSubdatasetName;
	  rsModAVUMetadata(rei->rsComm, &modAVUMetadataInp);
	  
	}
    }
  
  return;
  
}

void geoMetadata::extractMetaNetCDF()
{
  if( poDataset == NULL )
    {      
      rodsLog(LOG_ERROR, "Error occurred during geotiff metadata extraction : null dataset");
      rei->status = -1;
      return;
    }
  
  extractRasterBasicMeta();
  
  int ncid;
  int ndims;			/* number of dimensions */
  int nvars;			/* number of variables */
  int ngatts;			/* number of global attributes */
  int xdimid;			/* id of unlimited dimension */
  int varid;			/* variable id */
  int id;			/* dimension number per variable */
  int ia;			/* attribute number */
  int iv;			/* variable number */
  int dimid;			/* dimension id */
  int is_root = 1;		/* true if ncid is root group or if netCDF-3 */
  
  nc_open(filePath,NC_NOWRITE,&ncid);
  
  nc_inq(ncid, &ndims, &nvars, &ngatts, &xdimid);
  
  char attname[NC_MAX_NAME];
  void *attval;
  size_t attlen;
  nc_type atttype;
  char metaname[128];
  
  for (ia = 0; ia < ngatts; ia++)
    {
      nc_inq_attname(ncid, NC_GLOBAL, ia, attname); 
      if(strcasecmp(attname,"title") == 0)
	{
	  nc_inq_att(ncid,NC_GLOBAL,attname,&atttype,&attlen);
	  attval = (void *) malloc ((attlen + 1) * sizeof(char));
	  nc_get_att(ncid,NC_GLOBAL,attname,attval);
	  ((char *)attval)[attlen] = '\0';
	  snprintf(metaname, sizeof metaname, "title");
	  addMeta(metaname,(char *)attval);
	  snprintf(metaname, sizeof metaname, "description");
	  addMeta(metaname,(char *)attval);
	}
      if((strcasecmp(attname,"history") == 0) || (strcasecmp(attname,"source") == 0))
	{
	  nc_inq_att(ncid,NC_GLOBAL,attname,&atttype,&attlen);
	  attval = (void *) malloc ((attlen + 1) * sizeof(char));
	  nc_get_att(ncid,NC_GLOBAL,attname,attval);
	  ((char *)attval)[attlen] = '\0';
	  snprintf(metaname, sizeof metaname, "source");
	  addMeta(metaname,(char *)attval);
	}
    }
  
  //Call geoMetadata::setMeta to set previously extracted metadata to 
  //the iRODS file
  
  rei->status  = setMeta();
  
  if (nvars > 0) 
    {
      
      char *varname;
      nc_type vartype;
      int vardims,varnatts;
      char op[10];
      int status;
      
      snprintf(op, sizeof op, "add");
      
      for (varid = 0; varid < nvars; varid++) {
	varname = (char *) malloc(NC_MAX_NAME + 1);
	nc_inq_varname(ncid, varid, varname);
	
	modAVUMetadataInp_t modAVUMetadataInp;
	bzero (&modAVUMetadataInp, sizeof (modAVUMetadataInp));
	modAVUMetadataInp.arg0 = op;
	modAVUMetadataInp.arg1 = objType;
	modAVUMetadataInp.arg2 = objName;
	
	if (nc_inq_att(ncid, varid, "long_name", &atttype, &attlen) == NC_NOERR)
	  {
	    
	    attval = (void *) malloc((attlen + 1) * sizeof(char) );
	    nc_get_att(ncid, varid, "long_name", attval);
	    ((char *)attval)[attlen] = '\0';
	    
	    snprintf(metaname, sizeof metaname, "description");
	    modAVUMetadataInp.arg3 = metaname;
	    modAVUMetadataInp.arg4 = (char *) malloc(attlen + 1);
	    sprintf(modAVUMetadataInp.arg4, "%s", (char *)attval);
	    modAVUMetadataInp.arg5 = (char *) malloc(NC_MAX_NAME + 1);
	    sprintf(modAVUMetadataInp.arg5,"%s",varname);
	    rsModAVUMetadata(rei->rsComm, &modAVUMetadataInp);
		 
	    snprintf(metaname, sizeof metaname, "title");
	    modAVUMetadataInp.arg3 = metaname;
	    modAVUMetadataInp.arg4 = (char *) malloc(attlen + 1);
	    sprintf(modAVUMetadataInp.arg4, "%s", (char *)attval);
	    modAVUMetadataInp.arg5 = (char *) malloc(NC_MAX_NAME + 1);
	    sprintf(modAVUMetadataInp.arg5,"%s",varname);
	    rsModAVUMetadata(rei->rsComm, &modAVUMetadataInp);
	    
	  }
	
	snprintf(metaname, sizeof metaname, "subject");
	modAVUMetadataInp.arg3 = metaname;
	modAVUMetadataInp.arg4 = (char *) malloc(NC_MAX_NAME + 1);
	sprintf(modAVUMetadataInp.arg4,"%s",varname);
	modAVUMetadataInp.arg5 = (char *) malloc(NC_MAX_NAME + 1);
	sprintf(modAVUMetadataInp.arg5,"%s",varname);
	rsModAVUMetadata(rei->rsComm, &modAVUMetadataInp);
	
	free(varname);
	
      }         
    }
  
  nc_close(ncid);
  
  return;
}

int geoMetadata::extractGeoMeta()
{
  //call appropriate method based on geospatial file extension
  if(geoType == 1) //raster
    {
      if(strcmp(geoExt,".nc") == 0) 
	{
	  extractMetaNetCDF();
	  return rei->status;
	}
      else if(strcmp(geoExt,".tif") == 0)
	{
	  extractMetaGeoTiff();
	  return rei->status;
	}

      else
	{
	  //else unrecognized raster extension
	  //CANNOT HAPPEN since geoType is set to 1
	  rodsLog( LOG_ERROR, "msiExtractGeoMeta: Unrecognized raster file extension %s", geoExt );
	  return -1;
	}

    }
  else if(geoType == 2) //vector
    {
      extractMetaShp();
      return rei->status;
    }

  else
    {
      //else geoType = 0, so unrecognized/unsupported file type
      rodsLog( LOG_ERROR, "msiExtractGeoMeta: Unrecognized/Unsupported file extension %s", geoExt);
      return -1;
    }
}

//geospatial file extensions initialization
const std::vector<std::string> geoMetadata::rastertypes({".tif",".nc"});

//vectors need to include non-shp files to allow for metadata extractor to be called 
//on shp-associated files from acPostProcForPut rules since order of upload may 
//vary and we need to wait until these four files have been uploaded to extract
//shpfile metadata. See shapefileComplete for more details
const std::vector<std::string> geoMetadata::vectortypes({".shp", ".shx", ".prj", ".dbf"});

extern "C" {

  // =-=-=-=-=-=-=-
  int msiExtractGeoMeta( msParam_t* src_obj, ruleExecInfo_t* rei ) {
    dataObjInp_t srcObjInp, *mySrcObjInp;		/* for parsing input object */
    dataObjInfo_t *dataObjInfoHead = NULL;

    // Sanity checks
    if ( !rei || !rei->rsComm ) {
      rodsLog( LOG_ERROR, "msiExtractGeoMeta: Input rei or rsComm is NULL." );
      return ( SYS_INTERNAL_NULL_INPUT_ERR );
    }
    
    // Get source file object
    rei->status = parseMspForDataObjInp( src_obj, &srcObjInp, &mySrcObjInp, 0 );
    if ( rei->status < 0 ) {
      rodsLog( LOG_ERROR, "msiExtractGeoMeta: Input object error. status = %d", rei->status );
      return ( rei->status );
    }

    //get path to source object
    getDataObjInfo(rei->rsComm, mySrcObjInp, &dataObjInfoHead, NULL, 1);

    // Create geoMetadata instance
    geoMetadata myGeoMetadata (rei, dataObjInfoHead->objPath, dataObjInfoHead->filePath);
    
    // Call geoMetadata::extractGeoMeta
    rei->status = myGeoMetadata.extractGeoMeta();
    
    // Done
    return rei->status;
    
  }
  
  // =-=-=-=-=-=-=-
  // 2.  Create the plugin factory function which will return a microservice
  //     table entry
  irods::ms_table_entry*  plugin_factory() {
    // =-=-=-=-=-=-=-
    // 3. allocate a microservice plugin which takes the number of function
    //    params as a parameter to the constructor
    irods::ms_table_entry* msvc = new irods::ms_table_entry( 1 );
    
    // =-=-=-=-=-=-=-
    // 4. add the microservice function as an operation to the plugin
    //    the first param is the name / key of the operation, the second
    //    is the name of the function which will be the microservice
    msvc->add_operation( "msiExtractGeoMeta", "msiExtractGeoMeta" );
    
    // =-=-=-=-=-=-=-
    // 5. return the newly created microservice plugin
    return msvc;
  }
  
}; // extern "C"

