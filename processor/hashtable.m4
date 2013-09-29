dnl DEF_HASH_TABLE_FUNC(1:name, 2:datatype [, 3:dataname] [, 4:check_function])
define(`DEF_HASH_TABLE_FUNC', `
	DEF_STRUCT_P_FUNC(`HashTable', `$1', `
		pushdefFUNC_NAME(`$2', `$3')
		dnl {{{ dasm
		IFDASM(`
			const Bucket *srcBucket;
			zval *zv;
			int bufsize = 2;
			char *buf = emalloc(bufsize);
			int keysize;

#if defined(HARDENING_PATCH_HASH_PROTECT) && HARDENING_PATCH_HASH_PROTECT
			DONE(canary)
#endif
			DONE(nTableSize)
			DONE(nTableMask)
			DONE(nNumOfElements)
			DONE(nNextFreeElement)
			DONE(pInternalPointer)
			DONE(pListHead)
			DONE(pListTail)
			DONE(arBuckets)
			DONE(pDestructor)
			DONE(persistent)
			DONE(nApplyCount)
			DONE(bApplyProtection)
#if ZEND_DEBUG
			DONE(inconsistent)
#endif
#ifdef IS_UNICODE
			DONE(unicode)
#endif

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
		', `
		dnl }}}
		Bucket *srcBucket;
		IFCOPY(`Bucket *pnew = NULL, *prev = NULL;')
		zend_bool first = 1;
		dnl only used for copy
		IFCOPY(`uint n;')
		IFCALCCOPY(`size_t bucketsize;')

#if defined(HARDENING_PATCH_HASH_PROTECT) && HARDENING_PATCH_HASH_PROTECT
		IFRESTORE(`DST(`canary') = zend_hash_canary; DONE(canary)', `
			dnl else
			PROCESS(unsigned int, canary)
		')
#endif
		PROCESS(uint, nTableSize)
		PROCESS(uint, nTableMask)
		PROCESS(uint, nNumOfElements)
		PROCESS(ulong, nNextFreeElement)
		IFCOPY(`DST(`pInternalPointer') = NULL;	/* Used for element traversal */') DONE(pInternalPointer)
		IFCOPY(`DST(`pListHead') = NULL;') DONE(pListHead)
#ifdef ZEND_ENGINE_2_4
	if (SRC(`nTableMask')) {
#endif
		CALLOC(`DST(`arBuckets')', Bucket*, SRC(`nTableSize'))
		DONE(arBuckets)
		DISABLECHECK(`
		for (srcBucket = SRC(`pListHead'); srcBucket != NULL; srcBucket = srcBucket->pListNext) {
			ifelse($4, `', `', `
				pushdef(`BUCKET', `srcBucket')
				if ($4 == ZEND_HASH_APPLY_REMOVE) {
					IFCOPY(`DST(`nNumOfElements') --;')
					continue;
				}
				popdef(`BUCKET')
			')

			IFCALCCOPY(`bucketsize = BUCKET_SIZE(srcBucket);')
			ALLOC(pnew, char, bucketsize, , Bucket)
			IFCOPY(`
#ifdef ZEND_ENGINE_2_4
				memcpy(pnew, srcBucket, BUCKET_HEAD_SIZE(Bucket));
				if (BUCKET_KEY_SIZE(srcBucket)) {
					memcpy((char *) (pnew + 1), srcBucket->arKey, BUCKET_KEY_SIZE(srcBucket));
					pnew->arKey = (const char *) (pnew + 1);
				}
				else {
					pnew->arKey = NULL;
				}
#else
				memcpy(pnew, srcBucket, bucketsize);
#endif
				n = srcBucket->h & SRC(`nTableMask');
				/* pnew into hash node chain */
				pnew->pLast = NULL;
				pnew->pNext = DST(`arBuckets[n]');
				if (pnew->pNext) {
					pnew->pNext->pLast = pnew;
				}
				DST(`arBuckets[n]') = pnew;
			')
			IFDPRINT(`
				INDENT()
				fprintf(stderr, "$2:\"");
				xc_dprint_str_len(BUCKET_KEY_S(srcBucket), BUCKET_KEY_SIZE(srcBucket));
				fprintf(stderr, "\" %d:h=%lu ", BUCKET_KEY_SIZE(srcBucket), srcBucket->h);
			')
			if (sizeof(void *) == sizeof($2)) {
				IFCOPY(`pnew->pData = &pnew->pDataPtr;')
				dnl no alloc
				STRUCT_P_EX(`$2', pnew->pData, (($2*)srcBucket->pData), `', `$3', ` ')
			}
			else {
				STRUCT_P_EX(`$2', pnew->pData, (($2*)srcBucket->pData), `', `$3')
				IFCOPY(`pnew->pDataPtr = NULL;')
			}

			if (first) {
				IFCOPY(`DST(`pListHead') = pnew;')
				first = 0;
			}

			IFCOPY(`
				/* flat link */
				pnew->pListLast = prev;
				pnew->pListNext = NULL;
				if (prev) {
					prev->pListNext = pnew;
				}
				prev = pnew;
			')
		}
		')
		dnl TODO: fix pointer on arBuckets[n]
		FIXPOINTER(Bucket *, arBuckets)
#ifdef ZEND_ENGINE_2_4
	}
	else { /* if (SRC(`nTableMask')) */
		DONE(arBuckets)
	}
#endif
		IFCOPY(`DST(`pListTail') = pnew;') DONE(pListTail)
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
	')
')
