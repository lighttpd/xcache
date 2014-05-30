dnl DEF_HASH_TABLE_FUNC(1:name, 2:datatype [, 3:dataname])
define(`DEF_HASH_TABLE_FUNC', `DEF_STRUCT_P_FUNC(`HashTable', `$1', `
	pushdefFUNC_NAME(`$2', `$3')
	dnl {{{ dasm
	IFDASM(`
		const Bucket *srcBucket;
		zval *zv;
		int bufsize = 2;
		char *buf = emalloc(bufsize);
		int keysize;

		define(`AUTOCHECK_SKIP')
		IFAUTOCHECK(`xc_autocheck_skip = 1;')

		DISABLECHECK(`
		for (srcBucket = SRC(`pListHead'); srcBucket != NULL; srcBucket = srcBucket->pListNext) {
			ALLOC_INIT_ZVAL(zv);
			array_init(zv);
			FUNC_NAME (dasm, zv, (($2*)srcBucket->pData) TSRMLS_CC);
			keysize = BUCKET_KEY_SIZE(srcBucket) + 2;
			if (keysize > bufsize) {
				do {
					bufsize *= 2;
				} while (keysize > bufsize);
				buf = erealloc(buf, bufsize);
			}
			memcpy(buf, BUCKET_KEY_S(srcBucket), keysize);
			buf[keysize - 2] = buf[keysize - 1] = ""[0];
			keysize = srcBucket->nKeyLength;
#ifdef IS_UNICODE
			if (BUCKET_KEY_TYPE(srcBucket) == IS_UNICODE) {
				if (buf[0] == ""[0] && buf[1] == ""[0]) {
					keysize ++;
				}
			} else
#endif
			{
				if (buf[0] == ""[0]) {
					keysize ++;
				}
			}
			add_u_assoc_zval_ex(dst, BUCKET_KEY_TYPE(srcBucket), ZSTR(buf), keysize, zv);
		}
		')

		efree(buf);
	', ` dnl IFDASM else
	dnl }}}
	Bucket *srcBucket;
	IFCOPY(`Bucket *first = NULL, *last = NULL;')
	IFRELOCATE(`Bucket *dstBucket = NULL;')
	IFRESTORE(`Bucket *dstBucket = NULL;')
	IFRELOCATE(`uint n;')
	IFRESTORE(`uint n;')
	IFCALCCOPY(`size_t bucketSize;')

#if defined(HARDENING_PATCH_HASH_PROTECT) && HARDENING_PATCH_HASH_PROTECT
	IFRESTORE(`
		DST(`canary') = zend_hash_canary;
		DONE(canary)
	', `
		PROCESS(unsigned int, canary)
	')
#endif
	PROCESS(uint, nTableSize)
	PROCESS(uint, nTableMask)
	PROCESS(uint, nNumOfElements)
	PROCESS(ulong, nNextFreeElement)
	IFCOPY(`DST(`pInternalPointer') = NULL;	/* Used for element traversal */') DONE(pInternalPointer)
#ifdef ZEND_ENGINE_2_4
	if (SRC(`nTableMask')) {
#endif
		CALLOC(`DST(`arBuckets')', Bucket*, SRC(`nTableSize'))
		DONE(arBuckets)
		DISABLECHECK(`
		for (srcBucket = PTR_FROM_VIRTUAL_EX(`Bucket', SRC(`pListHead')); srcBucket != NULL; srcBucket = PTR_FROM_VIRTUAL_EX(`Bucket', `srcBucket->pListNext')) {
			IFCALCCOPY(`bucketSize = BUCKET_SIZE(srcBucket);')
			ALLOC(dstBucket, char, bucketSize, , Bucket)
			IFCOPY(`
#ifdef ZEND_ENGINE_2_4
				memcpy(dstBucket, srcBucket, BUCKET_HEAD_SIZE(Bucket));
				if (BUCKET_KEY_SIZE(srcBucket)) {
					memcpy((char *) (dstBucket + 1), srcBucket->arKey, BUCKET_KEY_SIZE(srcBucket));
					dstBucket->arKey = (const char *) (dstBucket + 1);
				}
				else {
					dstBucket->arKey = NULL;
				}
#else
				memcpy(dstBucket, srcBucket, bucketSize);
#endif
				n = srcBucket->h & SRC(`nTableMask');
				/* dstBucket into hash node chain */
				dstBucket->pLast = NULL;
				dstBucket->pNext = DST(`arBuckets[n]');
				if (dstBucket->pNext) {
					dstBucket->pNext->pLast = dstBucket;
				}
			')

			IFDPRINT(`
				INDENT()
				fprintf(stderr, "$2:\"");
				xc_dprint_str_len(BUCKET_KEY_S(srcBucket), BUCKET_KEY_SIZE(srcBucket));
				fprintf(stderr, "\" %d:h=%lu ", BUCKET_KEY_SIZE(srcBucket), srcBucket->h);
			')
			if (sizeof(void *) == sizeof($2)) {
				IFCOPY(`dstBucket->pData = &dstBucket->pDataPtr;')
				IFRELOCATE(`srcBucket->pData = &srcBucket->pDataPtr;')
				dnl $6 = ` ' to skip alloc, skip pointer fix
				STRUCT_P_EX(`$2', dstBucket->pData, (($2*)srcBucket->pData), `', `$3', ` ')
				RELOCATE_EX(`$2', dstBucket->pData)
			}
			else {
				STRUCT_P_EX(`$2', dstBucket->pData, (($2*)srcBucket->pData), `', `$3')
				IFCOPY(`dstBucket->pDataPtr = NULL;')
			}

			IFCOPY(`
				if (!first) {
					first = dstBucket;
				}

				/* flat link */
				dstBucket->pListLast = last;
				dstBucket->pListNext = NULL;
				if (last) {
					last->pListNext = dstBucket;
				}
				last = dstBucket;

				n = srcBucket->h & SRC(`nTableMask');
				/* dstBucket into hash node chain */
				dstBucket->pLast = NULL;
				dstBucket->pNext = DST(`arBuckets[n]');
				if (dstBucket->pNext) {
					dstBucket->pNext->pLast = dstBucket;
				}
				DST(`arBuckets[n]') = dstBucket;
			')
		}
		') dnl DISABLECHECK
		IFCOPY(`DST(`pListHead') = first;') DONE(pListHead)
		IFCOPY(`DST(`pListTail') = dstBucket;') DONE(pListTail)

		IFRELOCATE(`
		for (n = 0; n < SRC(`nTableSize'); ++n) {
			if (SRC(`arBuckets[n]')) {
				Bucket *next = PTR_FROM_VIRTUAL_EX(`Bucket', `DST(`arBuckets[n]')');
				do {
						dstBucket = next;
						next = PTR_FROM_VIRTUAL_EX(`Bucket', `next->pNext');
						if (dstBucket->pListLast) {
							RELOCATE_EX(Bucket, dstBucket->pListLast)
						}
						if (dstBucket->pListNext) {
							RELOCATE_EX(Bucket, dstBucket->pListNext)
						}
						if (dstBucket->pNext) {
							RELOCATE_EX(Bucket, dstBucket->pNext)
						}
						if (dstBucket->pLast) {
							RELOCATE_EX(Bucket, dstBucket->pLast)
						}
				} while (next);

				RELOCATE(Bucket, arBuckets[n])
			}
		}
		')
		if (SRC(`nNumOfElements')) {
			RELOCATE(Bucket, pListHead)
			RELOCATE(Bucket, pListTail)
		}
		RELOCATE(Bucket *, arBuckets)
#ifdef ZEND_ENGINE_2_4
	}
	else { /* if (SRC(`nTableMask')) */
		IFCOPY(`DST(`pListHead') = NULL;') DONE(pListHead)
		IFCOPY(`DST(`pListTail') = NULL;') DONE(pListTail)
		DONE(arBuckets)
	}
#endif
	IFCOPY(`DST(`pDestructor') = SRC(`pDestructor');') DONE(pDestructor)
	PROCESS(zend_bool, persistent)
#ifdef IS_UNICODE
	PROCESS(zend_bool, unicode)
#endif
	PROCESS(unsigned char, nApplyCount)
	PROCESS(zend_bool, bApplyProtection)
#if ZEND_DEBUG
	PROCESS(int, inconsistent)
#endif
	')dnl IFDASM
	popdef(`FUNC_NAME')
')')
