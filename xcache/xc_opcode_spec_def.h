static const xc_opcode_spec_t xc_opcode_spec[] = {
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 0 NOP                            */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 1 ADD                            */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 2 SUB                            */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 3 MUL                            */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 4 DIV                            */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 5 MOD                            */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 6 SL                             */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 7 SR                             */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 8 CONCAT                         */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 9 BW_OR                          */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 10 BW_AND                         */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 11 BW_XOR                         */
	OPSPEC(    UNUSED,        STD,     UNUSED,        TMP) /* 12 BW_NOT                         */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 13 BOOL_NOT                       */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 14 BOOL_XOR                       */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 15 IS_IDENTICAL                   */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 16 IS_NOT_IDENTICAL               */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 17 IS_EQUAL                       */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 18 IS_NOT_EQUAL                   */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 19 IS_SMALLER                     */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 20 IS_SMALLER_OR_EQUAL            */
	OPSPEC(      CAST,        STD,     UNUSED,        TMP) /* 21 CAST                           */
	OPSPEC(    UNUSED,        STD,     UNUSED,        TMP) /* 22 QM_ASSIGN                      */
#ifdef ZEND_ENGINE_2
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 23 ASSIGN_ADD                     */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 24 ASSIGN_SUB                     */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 25 ASSIGN_MUL                     */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 26 ASSIGN_DIV                     */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 27 ASSIGN_MOD                     */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 28 ASSIGN_SL                      */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 29 ASSIGN_SR                      */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 30 ASSIGN_CONCAT                  */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 31 ASSIGN_BW_OR                   */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 32 ASSIGN_BW_AND                  */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 33 ASSIGN_BW_XOR                  */
#else
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
	OPSPEC(    UNUSED,        VAR,        STD,        VAR)
#endif
	OPSPEC(    UNUSED,        VAR,     UNUSED,        VAR) /* 34 PRE_INC                        */
	OPSPEC(    UNUSED,        VAR,     UNUSED,        VAR) /* 35 PRE_DEC                        */
	OPSPEC(    UNUSED,        VAR,     UNUSED,        TMP) /* 36 POST_INC                       */
	OPSPEC(    UNUSED,        VAR,     UNUSED,        TMP) /* 37 POST_DEC                       */
	OPSPEC(    UNUSED,        VAR,        STD,        VAR) /* 38 ASSIGN                         */
	OPSPEC(    UNUSED,        VAR,        VAR,        VAR) /* 39 ASSIGN_REF                     */
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 40 ECHO                           */
	OPSPEC(    UNUSED,        STD,     UNUSED,        TMP) /* 41 PRINT                          */
#ifdef ZEND_ENGINE_2
	OPSPEC(    UNUSED,    JMPADDR,     UNUSED,     UNUSED) /* 42 JMP                            */
	OPSPEC(    UNUSED,        STD,    JMPADDR,     UNUSED) /* 43 JMPZ                           */
	OPSPEC(    UNUSED,        STD,    JMPADDR,     UNUSED) /* 44 JMPNZ                          */
#else
	OPSPEC(    UNUSED,     OPLINE,     UNUSED,     UNUSED)
	OPSPEC(    UNUSED,        STD,     OPLINE,     UNUSED)
	OPSPEC(    UNUSED,        STD,     OPLINE,     UNUSED)
#endif
	OPSPEC(    OPLINE,        STD,     OPLINE,     UNUSED) /* 45 JMPZNZ                         */
#ifdef ZEND_ENGINE_2
	OPSPEC(    UNUSED,        STD,    JMPADDR,        TMP) /* 46 JMPZ_EX                        */
	OPSPEC(    UNUSED,        STD,    JMPADDR,        TMP) /* 47 JMPNZ_EX                       */
#else
	OPSPEC(    UNUSED,        STD,     OPLINE,        TMP)
	OPSPEC(    UNUSED,        STD,     OPLINE,        TMP)
#endif
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 48 CASE                           */
	OPSPEC(       BIT,        STD,     UNUSED,     UNUSED) /* 49 SWITCH_FREE                    */
	OPSPEC(    UNUSED,        BRK,        STD,     UNUSED) /* 50 BRK                            */
	OPSPEC(    UNUSED,       CONT,        STD,     UNUSED) /* 51 CONT                           */
	OPSPEC(    UNUSED,        STD,     UNUSED,        TMP) /* 52 BOOL                           */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,        TMP) /* 53 INIT_STRING                    */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 54 ADD_CHAR                       */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 55 ADD_STRING                     */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 56 ADD_VAR                        */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,        TMP) /* 57 BEGIN_SILENCE                  */
	OPSPEC(    UNUSED,        TMP,     UNUSED,     UNUSED) /* 58 END_SILENCE                    */
	OPSPEC(INIT_FCALL,        STD,        STD,     UNUSED) /* 59 INIT_FCALL_BY_NAME             */
#ifdef ZEND_ENGINE_2
	OPSPEC(     FCALL,        STD,     OPLINE,        VAR) /* 60 DO_FCALL                       */
	OPSPEC(     FCALL,        STD,     OPLINE,        VAR) /* 61 DO_FCALL_BY_NAME               */
#else
	OPSPEC(     FCALL,        STD,     UNUSED,        VAR)
	OPSPEC(     FCALL,        STD,     UNUSED,        VAR)
#endif
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 62 RETURN                         */
	OPSPEC(    UNUSED,        ARG,     UNUSED,        VAR) /* 63 RECV                           */
	OPSPEC(    UNUSED,        ARG,        STD,        VAR) /* 64 RECV_INIT                      */
	OPSPEC(      SEND,        STD,        ARG,     UNUSED) /* 65 SEND_VAL                       */
	OPSPEC(      SEND,        VAR,        ARG,     UNUSED) /* 66 SEND_VAR                       */
	OPSPEC(      SEND,        VAR,        ARG,     UNUSED) /* 67 SEND_REF                       */
#ifdef ZEND_ENGINE_2
	OPSPEC(    UNUSED,      CLASS,     UNUSED,        VAR) /* 68 NEW                            */
#else
	OPSPEC(    UNUSED,        STD,     UNUSED,        VAR)
#endif
#ifdef ZEND_ENGINE_2_3
	OPSPEC(       STD,        STD,        STD,     UNUSED) /* 69 INIT_NS_FCALL_BY_NAME          */
#elif defined(ZEND_ENGINE_2_1)
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED)
#else
	OPSPEC(    UNUSED,        STD,     OPLINE,     UNUSED) /* 69 JMP_NO_CTOR                    */
#endif
	OPSPEC(    UNUSED,        TMP,     UNUSED,     UNUSED) /* 70 FREE                           */
	OPSPEC(       BIT,        STD,        STD,        TMP) /* 71 INIT_ARRAY                     */
	OPSPEC(       BIT,        STD,        STD,        TMP) /* 72 ADD_ARRAY_ELEMENT              */
	OPSPEC(    UNUSED,        STD,    INCLUDE,        VAR) /* 73 INCLUDE_OR_EVAL                */
#ifdef ZEND_ENGINE_2_1
  /* php 5.1 and up */
#	ifdef ZEND_ENGINE_2_4
	OPSPEC( FETCHTYPE,        STD,        STD,     UNUSED) /* 74 UNSET_VAR                      */
#	else
	OPSPEC(    UNUSED,        STD,      FETCH,     UNUSED) /* 74 UNSET_VAR                      */
#	endif
	OPSPEC(       STD,        STD,        STD,     UNUSED) /* 75 UNSET_DIM                      */
	OPSPEC(       STD,        STD,        STD,     UNUSED) /* 76 UNSET_OBJ                      */
	OPSPEC(       BIT,        STD,     OPLINE,        VAR) /* 77 FE_RESET                       */
#else
  /* <= php 5.0 */
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 74 UNSET_VAR                      */
  /* though there is no ISSET_ISEMPTY in php 5.0 it's better to leave it here i guess */
	OPSPEC(    UNUSED,        VAR,        STD,     UNUSED)
	OPSPEC(    UNUSED,        VAR,      ISSET,        TMP)
	OPSPEC(       BIT,        STD,     UNUSED,        VAR) /* 77 FE_RESET                       */
#endif
	OPSPEC(        FE,        STD,     OPLINE,        TMP) /* 78 FE_FETCH                       */
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 79 EXIT                           */
#ifdef ZEND_ENGINE_2_4
	OPSPEC( FETCHTYPE,        STD,        STD,        VAR) /* 80 FETCH_R                        */
#else
	OPSPEC(    UNUSED,        STD,      FETCH,        VAR) /* 80 FETCH_R                        */
#endif
	OPSPEC(     FETCH,        VAR,        STD,        VAR) /* 81 FETCH_DIM_R                    */
	OPSPEC(    UNUSED,      VAR_2,        STD,        VAR) /* 82 FETCH_OBJ_R                    */
#ifdef ZEND_ENGINE_2_4
	OPSPEC( FETCHTYPE,        STD,        STD,        VAR) /* 83 FETCH_W                        */
#else
	OPSPEC(    UNUSED,        STD,      FETCH,        VAR) /* 83 FETCH_W                        */
#endif
	OPSPEC(    UNUSED,        VAR,        STD,        VAR) /* 84 FETCH_DIM_W                    */
	OPSPEC(    UNUSED,      VAR_2,        STD,        VAR) /* 85 FETCH_OBJ_W                    */
#ifdef ZEND_ENGINE_2_4
	OPSPEC( FETCHTYPE,        STD,        STD,        VAR) /* 86 FETCH_RW                       */
#else
	OPSPEC(    UNUSED,        STD,      FETCH,        VAR) /* 86 FETCH_RW                       */
#endif
	OPSPEC(    UNUSED,        VAR,        STD,        VAR) /* 87 FETCH_DIM_RW                   */
	OPSPEC(    UNUSED,      VAR_2,        STD,        VAR) /* 88 FETCH_OBJ_RW                   */
#ifdef ZEND_ENGINE_2_4
	OPSPEC( FETCHTYPE,        STD,        STD,        VAR) /* 89 FETCH_IS                        */
#else
	OPSPEC(    UNUSED,        STD,      FETCH,        VAR) /* 89 FETCH_IS                       */
#endif
	OPSPEC(    UNUSED,        VAR,        STD,        VAR) /* 90 FETCH_DIM_IS                   */
	OPSPEC(    UNUSED,      VAR_2,        STD,        VAR) /* 91 FETCH_OBJ_IS                   */
#ifdef ZEND_ENGINE_2_4
	OPSPEC(       ARG,        STD,        STD,        VAR) /* 92 FETCH_FUNC_ARG                 */
#else
	OPSPEC(       ARG,        STD,      FETCH,        VAR) /* 92 FETCH_FUNC_ARG                 */
#endif
	OPSPEC(       ARG,        VAR,        STD,        VAR) /* 93 FETCH_DIM_FUNC_ARG             */
	OPSPEC(       ARG,      VAR_2,        STD,        VAR) /* 94 FETCH_OBJ_FUNC_ARG             */
#ifdef ZEND_ENGINE_2_4
	OPSPEC( FETCHTYPE,        STD,        STD,        VAR) /* 95 FETCH_UNSET                    */
#else
	OPSPEC(    UNUSED,        STD,      FETCH,        VAR) /* 95 FETCH_UNSET                    */
#endif
	OPSPEC(    UNUSED,        VAR,        STD,        VAR) /* 96 FETCH_DIM_UNSET                */
	OPSPEC(    UNUSED,      VAR_2,        STD,        VAR) /* 97 FETCH_OBJ_UNSET                */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 98 FETCH_DIM_TMP_VAR              */
#ifdef ZEND_ENGINE_2_3
	OPSPEC(    UNUSED,      VAR_2,        STD,        TMP) /* 99 FETCH_CONSTANT                 */
#elif defined(ZEND_ENGINE_2)
	OPSPEC(    UNUSED,     UCLASS,        STD,        TMP) /* 99 FETCH_CONSTANT                 */
#else
	OPSPEC(    UNUSED,        STD,     UNUSED,        TMP) /* 99 FETCH_CONSTANT                 */
#endif
#ifdef ZEND_ENGINE_2_3
	OPSPEC(       STD,    JMPADDR,        STD,     UNUSED) /* 100 GOTO                           */
#else
	OPSPEC(   DECLARE,        STD,        STD,     UNUSED) /* 100 DECLARE_FUNCTION_OR_CLASS      */
#endif
	OPSPEC(       STD,        STD,        STD,        STD) /* 101 EXT_STMT                       */
	OPSPEC(       STD,        STD,        STD,        STD) /* 102 EXT_FCALL_BEGIN                */
	OPSPEC(       STD,        STD,        STD,        STD) /* 103 EXT_FCALL_END                  */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 104 EXT_NOP                        */
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 105 TICKS                          */
	OPSPEC(SEND_NOREF,        VAR,        ARG,     UNUSED) /* 106 SEND_VAR_NO_REF                */
#ifndef ZEND_ENGINE_2
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 107 UNDEF                          */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 108 UNDEF                          */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 109 UNDEF                          */
	OPSPEC(     FCALL,        STD,     OPLINE,        VAR) /* 61 DO_FCALL_BY_FUNC                */
	OPSPEC(INIT_FCALL,        STD,        STD,     UNUSED) /* 111 INIT_FCALL_BY_FUNC             */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 112 UNDEF                          */
#else /* ZEND_ENGINE_2 */
#	ifdef ZEND_ENGINE_2_4
	OPSPEC(    OPLINE,        STD,        STD,     UNUSED) /* 107 CATCH                          */
#	else
	OPSPEC(    OPLINE,      CLASS,        STD,     UNUSED) /* 107 CATCH                          */
#	endif
	OPSPEC(    UNUSED,        STD,     OPLINE,     UNUSED) /* 108 THROW                          */
	OPSPEC(    FCLASS,        STD,        STD,      CLASS) /* 109 FETCH_CLASS                    */
	OPSPEC(    UNUSED,        STD,     UNUSED,        VAR) /* 110 CLONE                          */

#	ifdef ZEND_ENGINE_2_4
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 111 RETURN_BY_REF                  */
#	else
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 111 INIT_CTOR_CALL                 */
#	endif

	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 112 INIT_METHOD_CALL               */
#	ifdef ZEND_ENGINE_2_3
	OPSPEC(    UNUSED,        STD,        STD,     UNUSED) /* 113 INIT_STATIC_METHOD_CALL        */
#	else
	OPSPEC(    UNUSED,     UCLASS,        STD,     UNUSED) /* 113 INIT_STATIC_METHOD_CALL        */
#	endif
#	ifdef ZEND_ENGINE_2_4
	OPSPEC(     ISSET,        STD,        STD,        TMP) /* 114 ISSET_ISEMPTY_VAR              */
#	else
	OPSPEC(     ISSET,        STD,      FETCH,        TMP) /* 114 ISSET_ISEMPTY_VAR              */
#	endif
	OPSPEC(     ISSET,        STD,        STD,        TMP) /* 115 ISSET_ISEMPTY_DIM_OBJ          */

	OPSPEC(    UNUSED,      CLASS,        STD,     UNUSED) /* 116 IMPORT_FUNCTION                */
	OPSPEC(    UNUSED,      CLASS,        STD,     UNUSED) /* 117 IMPORT_CLASS                   */
	OPSPEC(    UNUSED,      CLASS,        STD,     UNUSED) /* 118 IMPORT_CONST                   */
	OPSPEC(       STD,        STD,        STD,        STD) /* 119 OP_119                         */
	OPSPEC(       STD,        STD,        STD,        STD) /* 120 OP_120                         */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 121 ASSIGN_ADD_OBJ                 */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 122 ASSIGN_SUB_OBJ                 */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 123 ASSIGN_MUL_OBJ                 */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 124 ASSIGN_DIV_OBJ                 */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 125 ASSIGN_MOD_OBJ                 */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 126 ASSIGN_SL_OBJ                  */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 127 ASSIGN_SR_OBJ                  */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 128 ASSIGN_CONCAT_OBJ              */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 129 ASSIGN_BW_OR_OBJ               */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 130 ASSIGN_BW_AND_OBJ              */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 131 ASSIGN_BW_XOR_OBJ              */

	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 132 PRE_INC_OBJ                    */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 133 PRE_DEC_OBJ                    */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 134 POST_INC_OBJ                   */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 135 POST_DEC_OBJ                   */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 136 ASSIGN_OBJ                     */
	OPSPEC(    UNUSED,        STD,        STD,        STD) /* 137 OP_DATA                        */
	OPSPEC(    UNUSED,        STD,      CLASS,        TMP) /* 138 INSTANCEOF                     */
	OPSPEC(    UNUSED,        STD,        STD,      CLASS) /* 139 DECLARE_CLASS                  */
	OPSPEC(     CLASS,        STD,        STD,      CLASS) /* 140 DECLARE_INHERITED_CLASS        */
	OPSPEC(    UNUSED,        STD,        STD,     UNUSED) /* 141 DECLARE_FUNCTION               */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 142 RAISE_ABSTRACT_ERROR           */
#	ifdef ZEND_ENGINE_2_3
	OPSPEC(   DECLARE,        STD,        STD,     UNUSED) /* 143 DECLARE_CONST                  */
#	else
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 143 UNDEF-143                      */
#	endif
#	ifdef ZEND_ENGINE_2_3
	OPSPEC(     IFACE,      CLASS,        STD,     UNUSED) /* 144 ADD_INTERFACE                  */
#	else
	OPSPEC(     IFACE,      CLASS,      CLASS,     UNUSED) /* 144 ADD_INTERFACE                  */
#	endif
#	ifdef ZEND_ENGINE_2_3
	OPSPEC(     CLASS,        STD,        STD,     OPLINE) /* 145 DECLARE_INHERITED_CLASS_DELAYED */
#	else
	OPSPEC(    UNUSED,      CLASS,        STD,     UNUSED) /* 145 VERIFY_INSTANCEOF              */
#	endif
	OPSPEC(    UNUSED,      CLASS,     UNUSED,     UNUSED) /* 146 VERIFY_ABSTRACT_CLASS          */
	OPSPEC(    UNUSED,        STD,        STD,        VAR) /* 147 ASSIGN_DIM                     */
	OPSPEC(     ISSET,        STD,        STD,        TMP) /* 148 ISSET_ISEMPTY_PROP_OBJ         */
	OPSPEC(       STD,     UNUSED,     UNUSED,        STD) /* 149 HANDLE_EXCEPTION               */
#endif /* ZEND_ENGINE_2 */
#ifdef ZEND_ENGINE_2_1
	OPSPEC(       STD,     UNUSED,     UNUSED,        STD) /* 150 USER_OPCODE                    */
#endif
#ifdef ZEND_ENGINE_2_3
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 151 UNDEF                          */
	OPSPEC(    UNUSED,        STD,    JMPADDR,        TMP) /* 152 JMP_SET                        */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 153 DECLARE_LAMBDA_FUNCTION        */
#endif
#ifdef ZEND_ENGINE_2_4
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 154 ADD_TRAIT                      */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 155 BIND_TRAITS                    */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 156 SEPARATE                       */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 157 QM_ASSIGN_VAR                  */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 158 JMP_SET_VAR                    */
#endif
#ifdef ZEND_ENGINE_2_5
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 159 DISCARD_EXCEPTION              */
	OPSPEC(    UNUSED,        STD,        STD,     UNUSED) /* 160 YIELD                          */
	OPSPEC(    UNUSED,        STD,     UNUSED,     UNUSED) /* 161 GENERATOR_RETURN               */
	OPSPEC(    UNUSED,    JMPADDR,     UNUSED,     UNUSED) /* 162 FAST_CALL                      */
	OPSPEC(    UNUSED,     UNUSED,     UNUSED,     UNUSED) /* 163 FAST_RET                       */
#endif
#ifdef ZEND_ENGINE_2_6
	OPSPEC(    UNUSED,        ARG,     UNUSED,        VAR) /* 164 RECV_VARIADIC                  */
	OPSPEC(      SEND,        STD,        ARG,     UNUSED) /* 165 SEND_UNPACK                    */
	OPSPEC(    UNUSED,        STD,        STD,        TMP) /* 166 POW                            */
	OPSPEC(    ASSIGN,        STD,        STD,        VAR) /* 167 ASSIGN_POW                     */
#endif
};
