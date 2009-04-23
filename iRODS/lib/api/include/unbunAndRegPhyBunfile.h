/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/* unbunAndRegPhyBunfile.h - This dataObj may be generated by a program or script
 */

#ifndef UNBUN_AND_REG_PHY_BUNFILE_H
#define UNBUN_AND_REG_PHY_BUNFILE_H

/* This is a Object File I/O call */

#include "rods.h"
#include "rcMisc.h"
#include "procApiRequest.h"
#include "apiNumber.h"
#include "initServer.h"
#include "dataObjWrite.h"
#include "dataObjClose.h"
#include "dataCopy.h"

#if defined(RODS_SERVER)
#define RS_UNBUN_AND_REG_PHY_BUNFILE rsUnbunAndRegPhyBunfile
/* prototype for the server handler */
int
rsUnbunAndRegPhyBunfile (rsComm_t *rsComm, dataObjInp_t *dataObjInp); 
int
_rsUnbunAndRegPhyBunfile (rsComm_t *rsComm, dataObjInp_t *dataObjInp,
rescInfo_t *rescInfo);
int
remoteUnbunAndRegPhyBunfile (rsComm_t *rsComm, dataObjInp_t *dataObjInp,
rodsServerHost_t *rodsServerHost);
int
unbunPhyBunFile (rsComm_t *rsComm, dataObjInp_t *dataObjInp,
rescInfo_t *rescInfo, char *bunFilePath, char *phyBunDir);
int
regUnbunPhySubfiles (rsComm_t *rsComm, rescInfo_t *rescInfo, char *phyBunDir,
int rmBunCopyFlag);
int
regPhySubFile (rsComm_t *rsComm, char *subfilePath,
dataObjInfo_t *bunDataObjInfo, rescInfo_t *rescInfo);
int
rmFilesInUnixDir (char *phyBunDir);
#else
#define RS_UNBUN_AND_REG_PHY_BUNFILE NULL
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/* prototype for the client call */
/* rcUnbunAndRegPhyBunfile - Unbundle a physical bundle file specified by
 * FILE_PATH_KW and register each subfile as replica. This call cannot be
 * called by normal users directly.
 * Input -
 *   rcComm_t *conn - The client connection handle.
 *   dataObjInp_t *dataObjInp - generic dataObj input. Relevant items are:
 *      objPath - the path of the data object.
 *      condInput - conditional Input
 *	    FILE_PATH_KW - the phyical path of the bundled file.
 *          DEST_RESC_NAME_KW - "value" = The destination Resource. 
 *   return value - The status of the operation.
 */
int
rcUnbunAndRegPhyBunfile (rcComm_t *conn, dataObjInp_t *dataObjInp);
#ifdef  __cplusplus
}
#endif

#endif	/* UNBUN_AND_REG_PHY_BUNFILE_H */
