#include "cgo_redis.h"

extern void initServerConfig(void);
extern void loadServerConfigFromString(char *config);
extern void createSharedObjects(void);

void initRedisServer(const void *buf, size_t len) {
  initServerConfig();
  createSharedObjects();
  if (buf != NULL && len != 0) {
    sds config = sdsnewlen(buf, len);
    loadServerConfigFromString(config);
    sdsfree(config);
  }
}

extern size_t cgoRedisRioRead(rio *rdb, void *buf, size_t len);
static size_t rioRedisRioRead(rio *rdb, void *buf, size_t len) {
  return cgoRedisRioRead(rdb, buf, len);
}

extern size_t cgoRedisRioWrite(rio *rdb, const void *buf, size_t len);
static size_t rioRedisRioWrite(rio *rdb, const void *buf, size_t len) {
  return cgoRedisRioWrite(rdb, buf, len);
}

extern off_t cgoRedisRioTell(rio *rdb);
static off_t rioRedisRioTell(rio *rdb) { return cgoRedisRioTell(rdb); }

extern int cgoRedisRioFlush(rio *rdb);
static int rioRedisRioFlush(rio *rdb) { return cgoRedisRioFlush(rdb); }

extern void cgoRedisRioUpdateChecksum(rio *rdb, uint64_t checksum);
static void rioRedisRioUpdateChecksum(rio *rdb, const void *buf, size_t len) {
  rioGenericUpdateChecksum(rdb, buf, len);
  cgoRedisRioUpdateChecksum(rdb, rdb->cksum);
}

static const rio redisRioIO = {
    rioRedisRioRead,
    rioRedisRioWrite,
    rioRedisRioTell,
    rioRedisRioFlush,
    rioRedisRioUpdateChecksum,
    0,           /* current checksum */
    0,           /* bytes read or written */
    8192,        /* read/write chunk size */
    {{NULL, 0}}, /* union for io-specific vars */
};

void redisRioInit(rio *rdb) { *rdb = redisRioIO; }

int redisRioRead(rio *rdb, void *buf, size_t len) {
  return rioRead(rdb, buf, len) != 0 ? 0 : -1;
}

int redisRioLoadLen(rio *rdb, uint64_t *len) {
  return (*len = rdbLoadLen(rdb, NULL)) != RDB_LENERR ? 0 : -1;
}

int redisRioLoadType(rio *rdb, int *typ) {
  return (*typ = rdbLoadType(rdb)) >= 0 ? 0 : -1;
}

int redisRioLoadTime(rio *rdb, time_t *val) {
  return (*val = rdbLoadTime(rdb)) >= 0 ? 0 : -1;
}

extern long long rdbLoadMillisecondTime(rio *rdb);

int redisRioLoadTimeMillisecond(rio *rdb, long long *val) {
  return (*val = rdbLoadMillisecondTime(rdb)) >= 0 ? 0 : -1;
}

void *redisRioLoadObject(rio *rdb, int typ) { return rdbLoadObject(typ, rdb); }
void *redisRioLoadStringObject(rio *rdb) { return rdbLoadStringObject(rdb); }

void redisSdsFree(void *ptr) { sdsfree(ptr); }

int redisObjectType(void *obj) { return ((robj *)obj)->type; }
int redisObjectEncoding(void *obj) { return ((robj *)obj)->encoding; }
int redisObjectRefCount(void *obj) { return ((robj *)obj)->refcount; }

void redisObjectIncrRefCount(void *obj) { incrRefCount(obj); }
void redisObjectDecrRefCount(void *obj) { decrRefCount(obj); }

extern void createDumpPayload(rio *payload, robj *o);

void *redisObjectCreateDumpPayload(void *obj, size_t *len) {
  rio payload;
  createDumpPayload(&payload, obj);
  sds buf = payload.io.buffer.ptr;
  *len = sdslen(buf);
  return buf;
}

extern int verifyDumpPayload(const char *buf, size_t len);

void *redisObjectDecodeFromPayload(void *buf, size_t len) {
  rio payload;
  if (verifyDumpPayload(buf, len) != C_OK) {
    return NULL;
  }
  int type;
  robj *obj = NULL;
  sds iobuf = sdsnewlen(buf, len);
  rioInitWithBuffer(&payload, iobuf);
  if ((type = rdbLoadObjectType(&payload)) != -1) {
    obj = rdbLoadObject(type, &payload);
  }
  sdsfree(iobuf);
  return obj;
}

size_t redisStringObjectLen(void *obj) {
  robj *o = obj;
  serverAssertWithInfo(NULL, o, o->type == OBJ_STRING);
  return stringObjectLen(o);
}

void *redisStringObjectUnsafeSds(void *obj, size_t *len, long long *val) {
  robj *o = obj;
  serverAssertWithInfo(NULL, o, o->type == OBJ_STRING);
  if (sdsEncodedObject(o)) {
    *len = sdslen(o->ptr);
    return o->ptr;
  } else if (o->encoding == OBJ_ENCODING_INT) {
    *val = (long)o->ptr;
    return NULL;
  }
  serverPanic("Unknown string encoding");
}

size_t redisListObjectLen(void *obj) {
  robj *o = obj;
  serverAssertWithInfo(NULL, o, o->type == OBJ_LIST);
  return listTypeLength(o);
}

void *redisListObjectNewIterator(void *obj) {
  robj *o = obj;
  serverAssertWithInfo(NULL, o, o->type == OBJ_LIST);
  return listTypeInitIterator(o, 0, LIST_TAIL);
}

void redisListIteratorRelease(void *iter) { listTypeReleaseIterator(iter); }

int redisListIteratorNext(void *iter, void **ptr, size_t *len, long long *val) {
  listTypeEntry entry;
  if (listTypeNext(iter, &entry)) {
    quicklistEntry *qe = &entry.entry;
    if (qe->value) {
      *ptr = qe->value;
      *len = qe->sz;
    } else {
      *val = qe->longval;
    }
    return 0;
  }
  return -1;
}

size_t redisHashObjectLen(void *obj) {
  robj *o = obj;
  serverAssertWithInfo(NULL, o, o->type == OBJ_HASH);
  return hashTypeLength(o);
}

size_t redisZsetObjectLen(void *obj) {
  robj *o = obj;
  serverAssertWithInfo(NULL, o, o->type == OBJ_ZSET);
  return zsetLength(o);
}

size_t redisSetObjectLen(void *obj) {
  robj *o = obj;
  serverAssertWithInfo(NULL, o, o->type == OBJ_SET);
  return setTypeSize(o);
}