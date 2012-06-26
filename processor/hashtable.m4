dnl DEF_HASH_TABLE_FUNC(1:name, 2:datatype [, 3:dataname] [, 4:check_function])
define(`DEF_HASH_TABLE_FUNC', `
	DEF_STRUCT_P_FUNC(`HashTable', `$1', `
		pushdefFUNC_NAME(`$2', `$3')
		dnl {{{ dasm
		IFDASM(`
			Bucket *b;
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
			for (b = src->pListHead; b != NULL; b = b->pListNext) {
				ALLOC_INIT_ZVAL(zv);
				array_init(zv);
				FUNC_NAME (dasm, zv, (($2*)b->pData) TSRMLS_CC);
				keysize = BUCKET_KEY_SIZE(b) + 2;
				if (keysize > bufsize) {
					do {
						bufsize *= 2;
					} while (keysize > bufsize);
					buf = erealloc(buf, bufsize);
				}
				memcpy(buf, BUCKET_KEY_S(b), keysize);
				buf[keysize - 2] = buf[keysize - 1] = ""[0];
				keysize = b->nKeyLength;
#ifdef IS_UNICODE
				if (BUCKET_KEY_TYPE(b) == IS_UNICODE) {
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
				add_u_assoc_zval_ex(dst, BUCKET_KEY_TYPE(b), ZSTR(buf), keysize, zv);
			}
			')

			efree(buf);
		', `
		dnl }}}
		Bucket *b, *pnew = NULL, *prev = NULL;
		zend_bool first = 1;
		dnl only used for copy
		IFCOPY(`uint n;')
		IFCALCCOPY(`int bucketsize;')

#if defined(HARDENING_PATCH_HASH_PROTECT) && HARDENING_PATCH_HASH_PROTECT
		IFASM(`dst->canary = zend_hash_canary; DONE(canary)', `
		dnl elseif
			IFRESTORE(`dst->canary = zend_hash_canary; DONE(canary)', `
				dnl else
				PROCESS(unsigned int, canary)
			')
		')
#endif
		PROCESS(uint, nTableSize)
		PROCESS(uint, nTableMask)
		PROCESS(uint, nNumOfElements)
		PROCESS(ulong, nNextFreeElement)
		IFCOPY(`dst->pInternalPointer = NULL;	/* Used for element traversal */') DONE(pInternalPointer)
		IFCOPY(`dst->pListHead = NULL;') DONE(pListHead)
#ifdef ZEND_ENGINE_2_4
		if (src->nTableMask) {
#endif
		CALLOC(dst->arBuckets, Bucket*, src->nTableSize)
		DONE(arBuckets)
		DISABLECHECK(`
		for (b = src->pListHead; b != NULL; b = b->pListNext) {
			ifelse($4, `', `', `
				pushdef(`BUCKET', `b')
				if ($4 == ZEND_HASH_APPLY_REMOVE) {
					IFCOPY(`dst->nNumOfElements --;')
					continue;
				}
				popdef(`BUCKET')
			')

			IFCALCCOPY(`bucketsize = BUCKET_SIZE(b);')
			ALLOC(pnew, char, bucketsize, , Bucket)
			IFCOPY(`
#ifdef ZEND_ENGINE_2_4
			memcpy(pnew, b, BUCKET_HEAD_SIZE(Bucket));
			memcpy((char *) (pnew + 1), b->arKey, BUCKET_KEY_SIZE(b));
			pnew->arKey = (const char *) (pnew + 1);
#else
			memcpy(pnew, b, bucketsize);
	#endif
			')
			IFCOPY(`
				n = b->h & src->nTableMask;
				/* pnew into hash node chain */
				pnew->pLast = NULL;
				pnew->pNext = dst->arBuckets[n];
				if (pnew->pNext) {
					pnew->pNext->pLast = pnew;
				}
				dst->arBuckets[n] = pnew;
			')
			IFDPRINT(`
				INDENT()
				fprintf(stderr, "$2:\"");
				xc_dprint_str_len(BUCKET_KEY_S(b), BUCKET_KEY_SIZE(b));
				fprintf(stderr, "\" %d:h=%lu ", BUCKET_KEY_SIZE(b), b->h);
			')
			if (sizeof(void *) == sizeof($2)) {
				IFCOPY(`pnew->pData = &pnew->pDataPtr;')
				dnl no alloc
				STRUCT_P_EX(`$2', pnew->pData, (($2*)b->pData), `', `$3', ` ')
			}
			else {
				STRUCT_P_EX(`$2', pnew->pData, (($2*)b->pData), `', `$3')
				IFCOPY(`pnew->pDataPtr = NULL;')
			}

			if (first) {
				IFCOPY(`dst->pListHead = pnew;')
				first = 0;
			}

			IFCOPY(`
				/* flat link */
				pnew->pListLast = prev;
				pnew->pListNext = NULL;
				if (prev) {
					prev->pListNext = pnew;
				}
			')
			prev = pnew;
		}
		')
#ifdef ZEND_ENGINE_2_4
	}
	else { /* if (src->nTableMask) */
		DONE(arBuckets)
	}
#endif
		IFCOPY(`dst->pListTail = pnew;') DONE(pListTail)
		IFCOPY(`dst->pDestructor = src->pDestructor;') DONE(pDestructor)
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
