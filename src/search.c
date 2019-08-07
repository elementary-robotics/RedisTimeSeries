#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "search.h"
#include "config.h"
#include "rmutil/alloc.h"
//#include "geo_index.h"

/***** Static functions *****/
static bool CheckFieldExist(RSLiteIndex *fti, const char *field, uint32_t fieldlen) {
    assert(fti);
    assert(field);

    for(uint32_t i = 0; i < fti->fields_count; ++i) {
        if (strncmp(field, fti->fields[i], fieldlen) == 0) {
            return true;
        }
    }
    return false;
} 

// Checks whether a field exists and if not, creates it
static void VerifyAddField(RSLiteIndex *fti, char *field, uint32_t fieldlen) {
    // TODO : change to AVL
    if (CheckFieldExist(fti, field, fieldlen) == false) {
        if (fti->fields_count % 64 == 0) {
            if (fti->fields_count == 0) {
                fti->fields = calloc(64, sizeof(char *));
            } else {
                fti->fields = realloc(fti->fields, sizeof(char *) * (fti->fields_count + 64));
            }
        }
        RediSearch_CreateField (fti->idx, field, 
                                RSFLDTYPE_FULLTEXT | RSFLDTYPE_NUMERIC | RSFLDTYPE_TAG,
                                RSFLDOPT_NONE);   
        fti->fields[fti->fields_count++] = RedisModule_Strdup(field);
        // Ensure retention of string
    }
}

/***** Modification functions *****/
RSLiteIndex *RSLiteCreate(const char *name) {
    RSLiteIndex *fti = (RSLiteIndex *)calloc(1, sizeof(RSLiteIndex));

    fti->fields = NULL; // TODO will probably changed once AVL is added
    fti->fields_count = 0;
    fti->idx = RediSearch_CreateIndex(name, NULL);

    return fti;
}

int RSL_Index(RSLiteIndex *fti, const char *item, uint32_t itemlen, 
              RSLabel *labels, count_t count) {
    RSDoc *doc = RediSearch_CreateDocument(item, itemlen, 1, 0);

    for(count_t i = 0; i < count; ++i) {
        VerifyAddField(fti, labels[i].fieldStr, labels[i].fieldLen);
            
        if (labels[i].RSFieldType == RSFLDTYPE_NUMERIC) {            
            RediSearch_DocumentAddFieldNumber(doc, labels[i].fieldStr,
                                labels[i].dbl, RSFLDTYPE_NUMERIC);
        } else if (labels[i].RSFieldType == RSFLDTYPE_FULLTEXT) {
            RediSearch_DocumentAddFieldString(doc, labels[i].fieldStr,
                                labels[i].valueStr, labels[i].valueLen, RSFLDTYPE_FULLTEXT);
        } else if (labels[i].RSFieldType == RSFLDTYPE_TAG) {
            RediSearch_DocumentAddFieldString(doc, labels[i].fieldStr,
                                labels[i].valueStr, labels[i].valueLen, RSFLDTYPE_TAG);
        } else if (labels[i].RSFieldType == RSFLDTYPE_GEO) {
            return REDISMODULE_ERR; // TODO error
        } else {
            return REDISMODULE_ERR; // TODO error
        }
    }
    RediSearch_SpecAddDocument(fti->idx, doc); // Always use ADD_REPLACE for simplicity
    return REDISMODULE_OK;
}

int RSL_Remove(RSLiteIndex *fti, const char *item, uint32_t itemlen) {
    return RediSearch_DeleteDocument(fti->idx, item, itemlen);
}

RSResultsIterator *RSL_GetQueryIter(RSLiteIndex *fti, const char *s, size_t n, char **err) {
  return RediSearch_IterateQuery(fti->idx, s, n, err);
}

const char *RSL_IterateResults(RSResultsIterator *iter, size_t *len) {
    return RediSearch_ResultsIteratorNext(iter, TSGlobalConfig.globalRSIndex->idx, len);
}
/*
void RSL_CreateQuery(const char *s, size_t n) {
int parsePredicate(RedisModuleCtx *ctx, RedisModuleString *label, QueryPredicate *retQuery, const char *separator) {
    char *token;
    char *iter_ptr;
    size_t _s;
    const char *labelRaw = RedisModule_StringPtrLen(label, &_s);
    char *labelstr = RedisModule_PoolAlloc(ctx, _s + 1);
    labelstr[_s] = '\0';
    strncpy(labelstr, labelRaw, _s);

    // Extract key
    token = strtok_r(labelstr, separator, &iter_ptr);
    if (token == NULL) {
        return TSDB_ERROR;
    }
    retQuery->key = RedisModule_CreateString(ctx, token, strlen(token));

    // Extract value
    token = strtok_r(NULL, separator, &iter_ptr);
    if (strstr(separator, "=(") != NULL) {
        if (token == NULL) {
            return TSDB_ERROR;
        }
        size_t token_len = strlen(token);

        if (token[token_len - 1] == ')') {
            token[token_len - 1] = '\0'; // remove closing parentheses
        } else {
            return TSDB_ERROR;
        }

        int filterCount = 0;
        for (int i=0; token[i]!='\0'; i++) {
            if (token[i] == ',') {
                filterCount++;
            }
        }
        if (token_len <= 1) {
            // when the token is <=1 it means that we have an empty list
            retQuery->valueListCount = 0;
        } else {
            retQuery->valueListCount = filterCount  + 1;
        }
        retQuery->valuesList = RedisModule_PoolAlloc(ctx, retQuery->valueListCount * sizeof(RedisModuleString*));

        char* subToken = strtok_r(token, ",", &iter_ptr);
        for (int i=0; i < retQuery->valueListCount; i++) {
            if (subToken == NULL) {
                continue;
            }
            retQuery->valuesList[i] = RedisModule_CreateStringPrintf(ctx, subToken);
            subToken = strtok_r(NULL, ",", &iter_ptr);
        }
    } else if (token != NULL) {
        retQuery->valueListCount = 1;
        retQuery->valuesList = RedisModule_PoolAlloc(ctx, sizeof(RedisModuleString*));
        retQuery->valuesList[0] = RedisModule_CreateString(ctx, token, strlen(token));
    } else {
        retQuery->valuesList = NULL;
        retQuery->valueListCount = 0;
    }
    return TSDB_OK;
}
*/

/****** Helper functions ******/
void FreeRSLabels(RSLabel *labels, size_t count, bool freeRMString) {
  for(size_t i = 0; i < count; ++i) {
    free(labels[i].fieldStr);
    free(labels[i].valueStr);
    if (freeRMString)
    {
        RedisModule_FreeString(NULL, labels[i].RTS_Label.key);
        RedisModule_FreeString(NULL, labels[i].RTS_Label.value);
    }    
  }
  
  free(labels);
}

Label *RSLabelToLabels(RSLabel *labels, size_t count) {
    Label *newLabels = (Label *)calloc(count, sizeof(Label));
    for(size_t i = 0; i < count; ++i) {
        newLabels[i].key = labels[i].RTS_Label.key;
        newLabels[i].value = labels[i].RTS_Label.value;
    }
    return newLabels;
}
