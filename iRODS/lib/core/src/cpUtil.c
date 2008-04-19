/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
#include <sys/time.h>
#include "rodsPath.h"
#include "rodsErrorTable.h"
#include "rodsLog.h"
#include "miscUtil.h"
#include "cpUtil.h"

int
cpUtil (rcComm_t *conn, rodsEnv *myRodsEnv, rodsArguments_t *myRodsArgs,
rodsPathInp_t *rodsPathInp)
{
    int i;
    int status; 
    int savedStatus = 0;
    rodsPath_t *targPath;
    dataObjCopyInp_t dataObjCopyInp;
    rodsRestart_t rodsRestart;

    if (rodsPathInp == NULL) {
	return (USER__NULL_INPUT_ERR);
    }

    status = resolveRodsTarget (conn, myRodsEnv, rodsPathInp, 1);
    if (status < 0) {
        rodsLogError (LOG_ERROR, status,
          "cpUtil: resolveRodsTarget error, status = %d", status);
        return (status);
    }

    initCondForCp (myRodsEnv, myRodsArgs, &dataObjCopyInp, &rodsRestart);

    for (i = 0; i < rodsPathInp->numSrc; i++) {
        targPath = &rodsPathInp->targPath[i];

        if (rodsPathInp->srcPath[i].rodsObjStat != NULL &&
          rodsPathInp->srcPath[i].rodsObjStat->specColl != NULL) {
            dataObjCopyInp.srcDataObjInp.specColl =
              rodsPathInp->srcPath[i].rodsObjStat->specColl;
        }

	if (targPath->objType == DATA_OBJ_T) {
	    status = cpFileUtil (conn, rodsPathInp->srcPath[i].outPath, 
	      targPath->outPath, rodsPathInp->srcPath[i].size, myRodsEnv, 
	       myRodsArgs, &dataObjCopyInp);
	} else if (targPath->objType == COLL_OBJ_T) {
            setStateForRestart (conn, &rodsRestart, targPath, myRodsArgs);
	    status = cpCollUtil (conn, rodsPathInp->srcPath[i].outPath,
              targPath->outPath, myRodsEnv, myRodsArgs, &dataObjCopyInp,
	      &rodsRestart);
           if (rodsRestart.fd > 0 && status < 0) {
                close (rodsRestart.fd);
                return (status);
            }
	    if (dataObjCopyInp.srcDataObjInp.specColl != NULL &&
              dataObjCopyInp.srcDataObjInp.specColl->class == STRUCT_FILE_COLL) {
                dataObjCopyInp.srcDataObjInp.specColl = NULL;
               status = cpCollUtil (conn, rodsPathInp->srcPath[i].outPath,
              targPath->outPath, myRodsEnv, myRodsArgs, &dataObjCopyInp,
              &rodsRestart);
            }
	} else {
	    /* should not be here */
	    rodsLog (LOG_ERROR,
	     "cpUtil: invalid cp dest objType %d for %s", 
	      targPath->objType, targPath->outPath);
	    return (USER_INPUT_PATH_ERR);
	}
	/* XXXX may need to return a global status */
	if (status < 0) {
	    rodsLogError (LOG_ERROR, status,
             "cpUtil: cp error for %s, status = %d", 
	      targPath->outPath, status);
            savedStatus = status;
	} 
    }

    if (rodsRestart.fd > 0) {
        close (rodsRestart.fd);
    }

    if (savedStatus < 0) {
        return (savedStatus);
    } else if (status == CAT_NO_ROWS_FOUND ||
      status == SYS_SPEC_COLL_OBJ_NOT_EXIST) {
        return (0);
    } else {
        return (status);
    }
}

int
cpFileUtil (rcComm_t *conn, char *srcPath, char *targPath, rodsLong_t srcSize,
rodsEnv *myRodsEnv, rodsArguments_t *rodsArgs, dataObjCopyInp_t *dataObjCopyInp)
{
    int status;
    struct timeval startTime, endTime;
 
    if (srcPath == NULL || targPath == NULL) {
       rodsLog (LOG_ERROR,
          "cpFileUtil: NULL srcPath or targPath incp");
        return (USER__NULL_INPUT_ERR);
    }

    if (rodsArgs->verbose == True) {
        (void) gettimeofday(&startTime, (struct timezone *)0);
    }

    rstrcpy (dataObjCopyInp->destDataObjInp.objPath, targPath, MAX_NAME_LEN);
    rstrcpy (dataObjCopyInp->srcDataObjInp.objPath, srcPath, MAX_NAME_LEN);
    dataObjCopyInp->srcDataObjInp.dataSize = srcSize;

    status = rcDataObjCopy (conn, dataObjCopyInp);

    if (status >= 0 && rodsArgs->verbose == True) {
        (void) gettimeofday(&endTime, (struct timezone *)0);
        printTiming (conn, dataObjCopyInp->destDataObjInp.objPath, 
	  conn->transStat.bytesWritten, NULL, &startTime, &endTime);
    }

    return (status);
}

int
initCondForCp (rodsEnv *myRodsEnv, rodsArguments_t *rodsArgs, 
dataObjCopyInp_t *dataObjCopyInp, rodsRestart_t *rodsRestart)
{
    if (dataObjCopyInp == NULL) {
       rodsLog (LOG_ERROR,
          "initCondForCp: NULL dataObjCopyInp incp");
        return (USER__NULL_INPUT_ERR);
    }

    memset (dataObjCopyInp, 0, sizeof (dataObjCopyInp_t));

    if (rodsArgs == NULL) {
	return (0);
    }

    if (rodsArgs->dataType == True) {
        if (rodsArgs->dataTypeString != NULL) {
	    addKeyVal (&dataObjCopyInp->destDataObjInp.condInput, DATA_TYPE_KW, 
	      rodsArgs->dataTypeString);
        }
    }

    if (rodsArgs->force == True) { 
        addKeyVal (&dataObjCopyInp->destDataObjInp.condInput, FORCE_FLAG_KW, "");
    }

    if (rodsArgs->checksum == True) {
        addKeyVal (&dataObjCopyInp->destDataObjInp.condInput, REG_CHKSUM_KW, "");
    }

    if (rodsArgs->verifyChecksum == True) {
        addKeyVal (&dataObjCopyInp->destDataObjInp.condInput, 
	 VERIFY_CHKSUM_KW, "");
    }

    if (rodsArgs->number == True) {
	if (rodsArgs->numberValue == 0) {
	    dataObjCopyInp->destDataObjInp.numThreads = NO_THREADING;
	} else {
	    dataObjCopyInp->destDataObjInp.numThreads = rodsArgs->numberValue;
	}
    }

    if (rodsArgs->physicalPath == True) {
	if (rodsArgs->physicalPathString == NULL) {
	    rodsLog (LOG_ERROR, 
	      "initCondForCp: NULL physicalPathString error");
	    return (USER__NULL_INPUT_ERR);
	} else {
            addKeyVal (&dataObjCopyInp->destDataObjInp.condInput, FILE_PATH_KW, 
	      rodsArgs->physicalPathString);
	}
    }

    if (rodsArgs->resource == True) {
        if (rodsArgs->resourceString == NULL) {
            rodsLog (LOG_ERROR,
              "initCondForCp: NULL resourceString error");
            return (USER__NULL_INPUT_ERR);
        } else {
            addKeyVal (&dataObjCopyInp->destDataObjInp.condInput, 
	      DEST_RESC_NAME_KW, rodsArgs->resourceString);
        }
    } else if (myRodsEnv != NULL && strlen (myRodsEnv->rodsDefResource) > 0) {
        addKeyVal (&dataObjCopyInp->destDataObjInp.condInput, DEST_RESC_NAME_KW,
          myRodsEnv->rodsDefResource);
    } 

    memset (rodsRestart, 0, sizeof (rodsRestart_t));
    if (rodsArgs->restart == True) {
        int status;
        status = openRestartFile (rodsArgs->restartFileString, rodsRestart,
          rodsArgs);
        if (status < 0) {
            rodsLogError (LOG_ERROR, status,
              "initCondForPut: openRestartFile of %s errno",
            rodsArgs->restartFileString);
            return (status);
        }
    }

    dataObjCopyInp->destDataObjInp.createMode = 0700;
    dataObjCopyInp->destDataObjInp.openFlags = O_WRONLY;

    return (0);
}

int
cpCollUtil (rcComm_t *conn, char *srcColl, char *targColl, 
rodsEnv *myRodsEnv, rodsArguments_t *rodsArgs, 
dataObjCopyInp_t *dataObjCopyInp, rodsRestart_t *rodsRestart)
{
    int status = 0;
    int savedStatus = 0;
    int collLen;
#if 0
    genQueryInp_t genQueryInp;
    genQueryOut_t *genQueryOut = NULL;
    dataObjInp_t dataObjOprInp;
    int rowInx;
    collSqlResult_t collSqlResult;
    collMetaInfo_t collMetaInfo;
    dataObjSqlResult_t dataObjSqlResult;
    dataObjMetaInfo_t dataObjMetaInfo;
#else
    collHandle_t collHandle;
    collEnt_t collEnt;
#endif
    char srcChildPath[MAX_NAME_LEN], targChildPath[MAX_NAME_LEN];

    if (srcColl == NULL || targColl == NULL) {
       rodsLog (LOG_ERROR,
          "cpCollUtil: NULL srcColl or targColl incp");
        return (USER__NULL_INPUT_ERR);
    }

    if (rodsArgs->recursive != True) {
        rodsLog (LOG_ERROR,
        "cpCollUtil: -r option must be used for cpting %s directory",
         srcColl);
        return (USER_INPUT_OPTION_ERR);
    }

    collLen = strlen (srcColl);

#if 0
    /* query all sub collections in srcColl and the mk the required
     * subdirectories */

    if (dataObjCopyInp->srcDataObjInp.specColl != NULL) {
        if (rodsArgs->verbose == True) {
            if (rodsArgs->verbose == True) {
                char objType[NAME_LEN];
                status = getSpecCollTypeStr (
		  dataObjCopyInp->srcDataObjInp.specColl, objType);
                if (status < 0) return (status);
                fprintf (stdout, "C- %s    %-5.5s :\n", targColl, objType);
            }
        }
        memset (&dataObjOprInp, 0, sizeof (dataObjOprInp));
        /* do the collection */
        addKeyVal (&dataObjOprInp.condInput, SEL_OBJ_TYPE_KW, "collection");
	dataObjOprInp.specColl = dataObjCopyInp->srcDataObjInp.specColl;
        rstrcpy (dataObjOprInp.objPath, srcColl, MAX_NAME_LEN);
        status = rcQuerySpecColl (conn, &dataObjOprInp, &genQueryOut);
    } else {
        if (rodsArgs->verbose == True) {
            fprintf (stdout, "C- %s:\n", targColl);
        }

        status = queryCollInCollReCur (conn, srcColl, rodsArgs, &genQueryInp,
          &genQueryOut);
    }

    if (status >= 0) {
        status = genQueryOutToCollRes (&genQueryOut, &collSqlResult);
    }

    rowInx = 0;
    while (status >= 0 &&
      (status = getNextCollMetaInfo (conn, &dataObjCopyInp->srcDataObjInp, 
      &genQueryInp, &collSqlResult, &rowInx, &collMetaInfo)) >= 0) {

        if (strlen (collMetaInfo.collName) <= collLen)
            continue;

        snprintf (targChildPath, MAX_NAME_LEN, "%s%s",
          targColl, collMetaInfo.collName + collLen);
        mkCollR (conn, targColl, targChildPath);

        if (rodsArgs->verbose == True) {
            fprintf (stdout, "C- %s:\n", targChildPath);
        }

        if (collMetaInfo.specColl.class != NO_SPEC_COLL) {
            /* the child is a spec coll. need to drill down */
            dataObjCopyInp_t childDataObjCopyInp;

            childDataObjCopyInp = *dataObjCopyInp;
            childDataObjCopyInp.srcDataObjInp.specColl = &collMetaInfo.specColl;
            status = cpCollUtil (conn, collMetaInfo.collName, targChildPath,
              myRodsEnv, rodsArgs, &childDataObjCopyInp, rodsRestart);

            if (status < 0 && status != CAT_NO_ROWS_FOUND &&
               status != SYS_SPEC_COLL_OBJ_NOT_EXIST) {
                return (status);
            }
        }
    }

    /* Now get all the files */

    if (dataObjCopyInp->srcDataObjInp.specColl != NULL) {
        addKeyVal (&dataObjOprInp.condInput, SEL_OBJ_TYPE_KW, "dataObj");
        status = rcQuerySpecColl (conn, &dataObjOprInp, &genQueryOut);

    } else {
        clearGenQueryInp (&genQueryInp);
        memset (&genQueryInp, 0, sizeof (genQueryInp));
        status = queryDataObjInCollReCur (conn, srcColl, rodsArgs, 
	  &genQueryInp, &genQueryOut);
    }

    if (status >= 0) {
        status = genQueryOutToDataObjRes (&genQueryOut, &dataObjSqlResult);
    }

    rowInx = 0;

    while (status >= 0 &&
      (status = getNextDataObjMetaInfo (conn, &dataObjCopyInp->srcDataObjInp, 
      &genQueryInp, &dataObjSqlResult, &rowInx, &collEnt)) >= 0) {

        snprintf (srcChildPath, MAX_NAME_LEN, "%s/%s",
          collEnt.collName, collEnt.dataName);
        snprintf (targChildPath, MAX_NAME_LEN, "%s%s/%s",
          targColl, collEnt.collName + collLen,
          collEnt.dataName);

        status = chkStateForResume (conn, rodsRestart, targChildPath,
          rodsArgs, DATA_OBJ_T, &dataObjCopyInp->destDataObjInp.condInput, 1);

        if (status < 0) {
            /* restart failed */
	    break;
        } else if (status == 0) {
            continue;
        }

        status = cpFileUtil (conn, srcChildPath, targChildPath,
          -1, myRodsEnv, rodsArgs, dataObjCopyInp);

        if (status < 0) {
            rodsLogError (LOG_ERROR, status,
              "getCollUtil: getDataObjUtil failed for %s. status = %d",
              srcChildPath, status);
            if (rodsRestart->fd > 0) {
                break;
            } else {
                savedStatus = status;
            }
        } else {
            status = procAndWrriteRestartFile (rodsRestart, targChildPath);
        }
    }

    if (dataObjCopyInp->srcDataObjInp.specColl == NULL) {
        clearGenQueryInp (&genQueryInp);
    }
#else
    status = rclOpenCollection (conn, srcColl, RECUR_QUERY_FG,
      &collHandle);

    if (status < 0) {
        rodsLog (LOG_ERROR,
          "getCollUtil: rclOpenCollection of %s error. status = %d",
          srcColl, status);
        return status;
    }
    while ((status = rclReadCollection (conn, &collHandle, &collEnt)) >= 0) {
        if (collEnt.objType == DATA_OBJ_T) {
            snprintf (srcChildPath, MAX_NAME_LEN, "%s/%s",
              collEnt.collName, collEnt.dataName);
            snprintf (targChildPath, MAX_NAME_LEN, "%s%s/%s",
              targColl, collEnt.collName + collLen,
              collEnt.dataName);

            status = chkStateForResume (conn, rodsRestart, targChildPath,
              rodsArgs, DATA_OBJ_T, &dataObjCopyInp->destDataObjInp.condInput,
	      1);

            if (status < 0) {
                /* restart failed */
                break;
            } else if (status == 0) {
                continue;
            }

            status = cpFileUtil (conn, srcChildPath, targChildPath,
              -1, myRodsEnv, rodsArgs, dataObjCopyInp);

            if (status < 0) {
                rodsLogError (LOG_ERROR, status,
                  "getCollUtil: getDataObjUtil failed for %s. status = %d",
                  srcChildPath, status);
                if (rodsRestart->fd > 0) {
                    break;
                } else {
                    savedStatus = status;
                }
            } else {
                status = procAndWrriteRestartFile (rodsRestart, targChildPath);
            }
        } else if (collEnt.objType == COLL_OBJ_T) {
            if (strlen (collEnt.collName) <= collLen)
                continue;

            snprintf (targChildPath, MAX_NAME_LEN, "%s%s",
              targColl, collEnt.collName + collLen);
            mkCollR (conn, targColl, targChildPath);

            if (rodsArgs->verbose == True) {
                fprintf (stdout, "C- %s:\n", targChildPath);
            }

            if (collEnt.specColl.class != NO_SPEC_COLL) {
                /* the child is a spec coll. need to drill down */
                dataObjCopyInp_t childDataObjCopyInp;

                childDataObjCopyInp = *dataObjCopyInp;
                childDataObjCopyInp.srcDataObjInp.specColl = 
		  &collEnt.specColl;
                status = cpCollUtil (conn, collEnt.collName, 
		  targChildPath, myRodsEnv, rodsArgs, &childDataObjCopyInp, 
		  rodsRestart);

                if (status < 0 && status != CAT_NO_ROWS_FOUND &&
                 status != SYS_SPEC_COLL_OBJ_NOT_EXIST) {
                    return (status);
                }
            }
        }
    }
    rclCloseCollection (&collHandle);

#endif

    if (savedStatus < 0) {
        return (savedStatus);
    } else if (status == CAT_NO_ROWS_FOUND ||
      status == SYS_SPEC_COLL_OBJ_NOT_EXIST) {
        return (0);
    } else {
        return (status);
    }
}

