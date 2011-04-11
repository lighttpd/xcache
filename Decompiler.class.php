<?php

define('INDENT', "\t");
ini_set('error_reporting', E_ALL);

function color($str, $color = 33)
{
	return "\x1B[{$color}m$str\x1B[0m";
}

function str($code, $indent = '') // {{{
{
	if (is_object($code)) {
		if (get_class($code) != 'Decompiler_Code') {
			$code = toCode($code, $indent);
		}
		return $code->__toString();
	}

	return (string) $code;
}
// }}}
function toCode($src, $indent = '') // {{{
{
	if (is_array($indent)) {
		$indent = $indent['indent'];
	}

	if (is_object($src)) {
		if (!method_exists($src, 'toCode')) {
			var_dump($src);
			exit('no toCode');
		}
		return new Decompiler_Code($src->toCode($indent));
	}

	return new Decompiler_Code($src);
}
// }}}
function value($value) // {{{
{
	$spec = xcache_get_special_value($value);
	if (isset($spec)) {
		$value = $spec;
		if (!is_array($value)) {
			// constant
			return $value;
		}
	}

	if (is_a($value, 'Decompiler_Object')) {
		// use as is
	}
	else if (is_array($value)) {
		$value = new Decompiler_ConstArray($value);
	}
	else {
		$value = new Decompiler_Value($value);
	}
	return $value;
}
// }}}
class Decompiler_Object // {{{
{
}
// }}}
class Decompiler_Value extends Decompiler_Object // {{{
{
	var $value;

	function Decompiler_Value($value = null)
	{
		$this->value = $value;
	}

	function toCode($indent)
	{
		return var_export($this->value, true);
	}
}
// }}}
class Decompiler_Code extends Decompiler_Object // {{{
{
	var $src;

	function Decompiler_Code($src)
	{
		$this->src = $src;
	}

	function toCode($indent)
	{
		return $this;
	}

	function __toString()
	{
		return $this->src;
	}
}
// }}}
class Decompiler_Binop extends Decompiler_Code // {{{
{
	var $opc;
	var $op1;
	var $op2;
	var $parent;
	var $indent;

	function Decompiler_Binop($parent, $op1, $opc, $op2)
	{
		$this->parent = &$parent;
		$this->opc = $opc;
		$this->op1 = $op1;
		$this->op2 = $op2;
	}

	function toCode($indent)
	{
		$op1 = toCode($this->op1, $indent);
		if (is_a($this->op1, 'Decompiler_Binop') && $this->op1->opc != $this->opc) {
			$op1 = "($op1)";
		}
		$opstr = $this->parent->binops[$this->opc];
		if ($op1 == '0' && $this->opc == XC_SUB) {
			return $opstr . toCode($this->op2, $indent);
		}
		return $op1 . ' ' . $opstr . ' ' . toCode($this->op2, $indent);
	}
}
// }}}
class Decompiler_Fetch extends Decompiler_Code // {{{
{
	var $src;
	var $fetchType;

	function Decompiler_Fetch($src, $type, $globalsrc)
	{
		$this->src = $src;
		$this->fetchType = $type;
		$this->globalsrc = $globalsrc;
	}

	function toCode($indent)
	{
		switch ($this->fetchType) {
		case ZEND_FETCH_LOCAL:
			return '$' . substr($this->src, 1, -1);
		case ZEND_FETCH_STATIC:
			die('static fetch cant to string');
		case ZEND_FETCH_GLOBAL:
		case ZEND_FETCH_GLOBAL_LOCK:
			return $this->globalsrc;
		default:
			var_dump($this->fetchType);
			assert(0);
		}
	}
}
// }}}
class Decompiler_Box // {{{
{
	var $obj;

	function Decompiler_Box(&$obj)
	{
		$this->obj = &$obj;
	}

	function toCode($indent)
	{
		return $this->obj->toCode($indent);
	}
}
// }}}
class Decompiler_Dim extends Decompiler_Value // {{{
{
	var $offsets = array();
	var $isLast = false;
	var $assign = null;

	function toCode($indent)
	{
		if (is_a($this->value, 'Decompiler_ListBox')) {
			$exp = toCode($this->value->obj->src, $indent);
		}
		else {
			$exp = toCode($this->value, $indent);
		}
		foreach ($this->offsets as $dim) {
			$exp .= '[' . toCode($dim, $indent) . ']';
		}
		return $exp;
	}
}
// }}}
class Decompiler_DimBox extends Decompiler_Box // {{{
{
}
// }}}
class Decompiler_List extends Decompiler_Code // {{{
{
	var $src;
	var $dims = array();
	var $everLocked = false;

	function toCode($indent)
	{
		if (count($this->dims) == 1 && !$this->everLocked) {
			$dim = $this->dims[0];
			unset($dim->value);
			$dim->value = $this->src;
			if (!isset($dim->assign)) {
				return toCode($dim, $indent);
			}
			return toCode($this->dims[0]->assign, $indent) . ' = ' . toCode($dim, $indent);
		}
		/* flatten dims */
		$assigns = array();
		foreach ($this->dims as $dim) {
			$assign = &$assigns;
			foreach ($dim->offsets as $offset) {
				$assign = &$assign[$offset];
			}
			$assign = toCode($dim->assign, $indent);
		}
		return $this->toList($assigns) . ' = ' . toCode($this->src, $indent);
	}

	function toList($assigns)
	{
		$keys = array_keys($assigns);
		if (count($keys) < 2) {
			$keys[] = 0;
		}
		$max = call_user_func_array('max', $keys);
		$list = 'list(';
		for ($i = 0; $i <= $max; $i ++) {
			if ($i) {
				$list .= ', ';
			}
			if (!isset($assigns[$i])) {
				continue;
			}
			if (is_array($assigns[$i])) {
				$list .= $this->toList($assigns[$i]);
			}
			else {
				$list .= $assigns[$i];
			}
		}
		return $list . ')';
	}
}
// }}}
class Decompiler_ListBox extends Decompiler_Box // {{{
{
}
// }}}
class Decompiler_Array extends Decompiler_Value // {{{
{
	// emenets
	function Decompiler_Array()
	{
		$this->value = array();
	}

	function toCode($indent)
	{
		$subindent = $indent . INDENT;

		$elementsCode = array();
		$index = 0;
		foreach ($this->value as $element) {
			list($key, $value) = $element;
			if (!isset($key)) {
				$key = $index++;
			}
			$elementsCode[] = array(str($key, $subindent), str($value, $subindent), $key, $value);
		}

		$exp = "array(";
		$indent = $indent . INDENT;
		$assocWidth = 0;
		$multiline = 0;
		$i = 0;
		foreach ($elementsCode as $element) {
			list($keyCode, $valueCode) = $element;
			if ((string) $i !== $keyCode) {
				$assocWidth = 1;
				break;
			}
			++$i;
		}
		foreach ($elementsCode as $element) {
			list($keyCode, $valueCode, $key, $value) = $element;
			if ($assocWidth) {
				$len = strlen($keyCode);
				if ($assocWidth < $len) {
					$assocWidth = $len;
				}
			}
			if (is_array($value) || is_a($value, 'Decompiler_Array')) {
				$multiline ++;
			}
		}

		$i = 0;
		foreach ($elementsCode as $element) {
			list($keyCode, $value) = $element;
			if ($multiline) {
				if ($i) {
					$exp .= ",";
				}
				$exp .= "\n";
				$exp .= $indent;
			}
			else {
				if ($i) {
					$exp .= ", ";
				}
			}

			if ($assocWidth) {
				if ($multiline) {
					$exp .= sprintf("%-{$assocWidth}s => ", $keyCode);
				}
				else {
					$exp .= $keyCode . ' => ';
				}
			}

			$exp .= $value;

			$i ++;
		}
		if ($multiline) {
			$exp .= "\n$indent)";
		}
		else {
			$exp .= ")";
		}
		return $exp;
	}
}
// }}}
class Decompiler_ConstArray extends Decompiler_Array // {{{
{
	function Decompiler_ConstArray($array)
	{
		$elements = array();
		foreach ($array as $key => $value) {
			$elements[] = array(value($key), value($value));
		}
		$this->value = $elements;
	}
}
// }}}
class Decompiler_ForeachBox extends Decompiler_Box // {{{
{
	var $iskey;

	function toCode($indent)
	{
		return 'foreach (' . '';
	}
}
// }}}

class Decompiler
{
	var $rName = '!^[\\w_][\\w\\d_]*$!';
	var $rQuotedName = "!^'[\\w_][\\w\\d_]*'\$!";

	function Decompiler()
	{
		// {{{ opinfo
		$this->unaryops = array(
				XC_BW_NOT   => '~',
				XC_BOOL_NOT => '!',
				);
		$this->binops = array(
				XC_ADD                 => "+",
				XC_ASSIGN_ADD          => "+=",
				XC_SUB                 => "-",
				XC_ASSIGN_SUB          => "-=",
				XC_MUL                 => "*",
				XC_ASSIGN_MUL          => "*=",
				XC_DIV                 => "/",
				XC_ASSIGN_DIV          => "/=",
				XC_MOD                 => "%",
				XC_ASSIGN_MOD          => "%=",
				XC_SL                  => "<<",
				XC_ASSIGN_SL           => "<<=",
				XC_SR                  => ">>",
				XC_ASSIGN_SR           => ">>=",
				XC_CONCAT              => ".",
				XC_ASSIGN_CONCAT       => ".=",
				XC_IS_IDENTICAL        => "===",
				XC_IS_NOT_IDENTICAL    => "!==",
				XC_IS_EQUAL            => "==",
				XC_IS_NOT_EQUAL        => "!=",
				XC_IS_SMALLER          => "<",
				XC_IS_SMALLER_OR_EQUAL => "<=",
				XC_BW_OR               => "|",
				XC_ASSIGN_BW_OR        => "|=",
				XC_BW_AND              => "&",
				XC_ASSIGN_BW_AND       => "&=",
				XC_BW_XOR              => "^",
				XC_ASSIGN_BW_XOR       => "^=",
				XC_BOOL_XOR            => "xor",
				);
		// }}}
		$this->includeTypes = array( // {{{
				ZEND_EVAL         => 'eval',
				ZEND_INCLUDE      => 'include',
				ZEND_INCLUDE_ONCE => 'include_once',
				ZEND_REQUIRE      => 'require',
				ZEND_REQUIRE_ONCE => 'require_once',
				);
				// }}}
	}
	function outputPhp(&$opcodes, $opline, $last, $indent) // {{{
	{
		$origindent = $indent;
		$curticks = 0;
		for ($i = $opline; $i <= $last; $i ++) {
			$op = $opcodes[$i];
			if (isset($op['php'])) {
				$toticks = isset($op['ticks']) ? $op['ticks'] : 0;
				if ($curticks != $toticks) {
					if (!$toticks) {
						echo $origindent, "}\n";
						$indent = $origindent;
					}
					else {
						if ($curticks) {
							echo $origindent, "}\n";
						}
						else if (!$curticks) {
							$indent .= INDENT;
						}
						echo $origindent, "declare(ticks=$curticks) {\n";
					}
					$curticks = $toticks;
				}
				echo $indent, toCode($op['php'], $indent), ";\n";
			}
		}
		if ($curticks) {
			echo $origindent, "}\n";
		}
	}
	// }}}
	function getOpVal($op, &$EX, $tostr = true, $free = false) // {{{
	{
		switch ($op['op_type']) {
		case XC_IS_CONST:
			return toCode(value($op['constant']), $EX);

		case XC_IS_VAR:
		case XC_IS_TMP_VAR:
			$T = &$EX['Ts'];
			$ret = $T[$op['var']];
			if ($tostr) {
				$ret = toCode($ret, $EX);
			}
			if ($free) {
				unset($T[$op['var']]);
			}
			return $ret;

		case XC_IS_CV:
			$var = $op['var'];
			$var = $EX['op_array']['vars'][$var];
			return '$' . $var['name'];

		case XC_IS_UNUSED:
			return null;
		}
	}
	// }}}
	function removeKeyPrefix($array, $prefix) // {{{
	{
		$prefixLen = strlen($prefix);
		$ret = array();
		foreach ($array as $key => $value) {
			if (substr($key, 0, $prefixLen) == $prefix) {
				$key = substr($key, $prefixLen);
			}
			$ret[$key] = $value;
		}
		return $ret;
	}
	// }}}
	function &fixOpcode($opcodes, $removeTailing = false, $defaultReturnValue = null) // {{{
	{
		for ($i = 0, $cnt = count($opcodes); $i < $cnt; $i ++) {
			if (function_exists('xcache_get_fixed_opcode')) {
				$opcodes[$i]['opcode'] = xcache_get_fixed_opcode($opcodes[$i]['opcode'], $i);
			}
			if (isset($opcodes[$i]['op1'])) {
				$opcodes[$i]['op1'] = $this->removeKeyPrefix($opcodes[$i]['op1'], 'u.');
				$opcodes[$i]['op2'] = $this->removeKeyPrefix($opcodes[$i]['op2'], 'u.');
				$opcodes[$i]['result'] = $this->removeKeyPrefix($opcodes[$i]['result'], 'u.');
			}
			else {
				$op = array(
					'op1' => array(),
					'op2' => array(),
					'op3' => array(),
				);
				foreach ($opcodes[$i] as $name => $value) {
					if (preg_match('!^(op1|op2|result)\\.(.*)!', $name, $m)) {
						list(, $which, $field) = $m;
						$op[$which][$field] = $value;
					}
					else if (preg_match('!^(op1|op2|result)_type$!', $name, $m)) {
						list(, $which) = $m;
						$op[$which]['op_type'] = $value;
					}
					else {
						$op[$name] = $value;
					}
				}
				$opcodes[$i] = $op;
			}
		}

		if ($removeTailing) {
			$last = count($opcodes) - 1;
			if ($opcodes[$last]['opcode'] == XC_HANDLE_EXCEPTION) {
				unset($opcodes[$last]);
				--$last;
			}
			if ($opcodes[$last]['opcode'] == XC_RETURN) {
				$op1 = $opcodes[$last]['op1'];
				if ($op1['op_type'] == XC_IS_CONST && array_key_exists('constant', $op1) && $op1['constant'] === $defaultReturnValue) {
					unset($opcodes[$last]);
					--$last;
				}
			}
		}
		return $opcodes;
	}
	// }}}
	function &dop_array($op_array, $indent = '') // {{{
	{
		$op_array['opcodes'] = $this->fixOpcode($op_array['opcodes'], true, $indent == '' ? 1 : null);
		$opcodes = &$op_array['opcodes'];
		$EX['indent'] = '';
		// {{{ build jmp array
		for ($i = 0, $cnt = count($opcodes); $i < $cnt; $i ++) {
			$op = &$opcodes[$i];
			/*
			if ($op['opcode'] == XC_JMPZ) {
				$this->dumpop($op, $EX);
				var_dump($op);
			}
			continue;
			*/
			$op['line'] = $i;
			switch ($op['opcode']) {
			case XC_JMP:
				$target = $op['op1']['var'];
				$op['jmpouts'] = array($target);
				$opcodes[$target]['jmpins'][] = $i;
				break;

			case XC_JMPZNZ:
				$jmpz = $op['op2']['opline_num'];
				$jmpnz = $op['extended_value'];
				$op['jmpouts'] = array($jmpz, $jmpnz);
				$opcodes[$jmpz]['jmpins'][] = $i;
				$opcodes[$jmpnz]['jmpins'][] = $i;
				break;

			case XC_JMPZ:
			case XC_JMPNZ:
			case XC_JMPZ_EX:
			case XC_JMPNZ_EX:
			// case XC_FE_RESET:
			case XC_FE_FETCH:
			// case XC_JMP_NO_CTOR:
				$target = $op['op2']['opline_num'];
				//if (!isset($target)) {
				//	$this->dumpop($op, $EX);
				//	var_dump($op); exit;
				//}
				$op['jmpouts'] = array($target);
				$opcodes[$target]['jmpins'][] = $i;
				break;

			/*
			case XC_RETURN:
				$op['jmpouts'] = array();
				break;
			*/
			}
		}
		unset($op);
		// }}}
		// build semi-basic blocks
		$nextbbs = array();
		$starti = 0;
		for ($i = 1, $cnt = count($opcodes); $i < $cnt; $i ++) {
			if (isset($opcodes[$i]['jmpins'])
			 || isset($opcodes[$i - 1]['jmpouts'])) {
				$nextbbs[$starti] = $i;
				$starti = $i;
			}
		}
		$nextbbs[$starti] = $cnt;

		$EX = array();
		$EX['Ts'] = array();
		$EX['indent'] = $indent;
		$EX['nextbbs'] = $nextbbs;
		$EX['op_array'] = &$op_array;
		$EX['opcodes'] = &$opcodes;
		// func call
		$EX['object'] = null;
		$EX['called_scope'] = null;
		$EX['fbc'] = null;
		$EX['argstack'] = array();
		$EX['arg_types_stack'] = array();
		$EX['last'] = count($opcodes) - 1;
		$EX['silence'] = 0;

		for ($next = 0, $last = $EX['last'];
				$loop = $this->outputCode($EX, $next, $last, $indent, true);
				list($next, $last) = $loop) {
			// empty
		}
		return $EX;
	}
	// }}}
	function outputCode(&$EX, $opline, $last, $indent, $loop = false) // {{{
	{
		$op = &$EX['opcodes'][$opline];
		$next = $EX['nextbbs'][$opline];

		$end = $next - 1;
		if ($end > $last) {
			$end = $last;
		}

		if (isset($op['jmpins'])) {
			echo "\nline", $op['line'], ":\n";
		}
		else {
			// echo ";;;\n";
		}
		$this->dasmBasicBlock($EX, $opline, $end);
		$this->outputPhp($EX['opcodes'], $opline, $end, $indent);
		// jmpout op
		$op = &$EX['opcodes'][$end];
		$op1 = $op['op1'];
		$op2 = $op['op2'];
		$ext = $op['extended_value'];
		$line = $op['line'];

		if (isset($EX['opcodes'][$next])) {
			if (isset($last) && $next > $last) {
				$next = null;
			}
		}
		else {
			$next = null;
		}
		if ($op['opcode'] == XC_FE_FETCH) {
			$opline = $next;
			$next = $op['op2']['opline_num'];
			$end = $next - 1;

			ob_start();
			$this->outputCode($EX, $opline, $end /* - 1 skip last jmp */, $indent . INDENT);
			$body = ob_get_clean();

			$as = toCode($op['fe_as'], $EX);
			if (isset($op['fe_key'])) {
				$as = toCode($op['fe_key'], $EX) . ' => ' . $as;
			}
			echo "{$indent}foreach (" . toCode($op['fe_src'], $EX) . " as $as) {\n";
			echo $body;
			echo "{$indent}}";
			// $this->outputCode($EX, $next, $last, $indent);
			// return;
		}
		/*
		if ($op['opcode'] == XC_JMPZ) {
			$target = $op2['opline_num'];
			if ($line + 1) {
				$nextblock = $EX['nextbbs'][$next];
				$jmpop = end($nextblock);
				if ($jmpop['opcode'] == XC_JMP) {
					$ifendline = $op2['opline_num'];
					if ($ifendline >= $line) {
						$cond = $op['cond'];
						echo "{$indent}if ($cond) {\n";
						$this->outputCode($EX, $next, $last, INDENT . $indent);
						echo "$indent}\n";
						$this->outputCode($EX, $target, $last, $indent);
						return;
					}
				}
			}
		}
		*/
		if (!isset($next)) {
			return;
		}
		if (!empty($op['jmpouts']) && isset($op['isjmp'])) {
			if (isset($op['cond'])) {
				echo "{$indent}check ($op[cond]) {\n";
				echo INDENT;
			}
			echo $indent;
			echo xcache_get_opcode($op['opcode']), ' line', $op['jmpouts'][0];
			if (isset($op['jmpouts'][1])) {
				echo ', line', $op['jmpouts'][1];
			}
			echo ";";
			// echo ' // <- line', $op['line'];
			echo "\n";
			if (isset($op['cond'])) echo "$indent}\n";
		}

		// proces JMPZ_EX/JMPNZ_EX for AND,OR
		$op = &$EX['opcodes'][$next];
		/*
		if (isset($op['jmpins'])) {
			foreach (array_reverse($op['jmpins']) as $fromline) {
				$fromop = $EX['opcodes'][$fromline];
				switch ($fromop['opcode']) {
				case XC_JMPZ_EX: $opstr = 'and'; break;
				case XC_JMPNZ_EX: $opstr = 'or'; break;
				case XC_JMPZNZ: var_dump($fromop); exit;
				default: continue 2;
				}

				$var = $fromop['result']['var'];
				var_dump($EX['Ts'][$var]);
				$EX['Ts'][$var] = '(' . $fromop['and_or'] . " $opstr " . $EX['Ts'][$var] . ')';
			}
			#$this->outputCode($EX, $next, $last, $indent);
			#return;
		}
		*/
		if (isset($op['cond_false'])) {
			// $this->dumpop($op, $EX);
			// any true comes here, so it's a "or"
			$cond = implode(' and ', $op['cond_false']);
			// var_dump($op['cond'] = $cond);
			/*
			$rvalue = implode(' or ', $op['cond_true']) . ' or ' . $rvalue;
			unset($op['cond_true']);
			*/
		}

		if ($loop) {
			return array($next, $last);
		}
		$this->outputCode($EX, $next, $last, $indent);
	}
	// }}}
	function unquoteName($str) // {{{
	{
		if (preg_match($this->rQuotedName, $str)) {
			$str = substr($str, 1, -1);
		}
		return $str;
	}
	// }}}
	function dasmBasicBlock(&$EX, $opline, $last) // {{{
	{
		$T = &$EX['Ts'];
		$opcodes = &$EX['opcodes'];
		$lastphpop = null;

		for ($i = $opline, $ic = $last + 1; $i < $ic; $i ++) {
			// {{{ prepair
			$op = &$opcodes[$i];
			$opc = $op['opcode'];
			if ($opc == XC_NOP) {
				continue;
			}

			$op1 = $op['op1'];
			$op2 = $op['op2'];
			$res = $op['result'];
			$ext = $op['extended_value'];

			$opname = xcache_get_opcode($opc);

			if ($opname == 'UNDEF' || !isset($opname)) {
				echo 'UNDEF OP:';
				$this->dumpop($op, $EX);
				continue;
			}
			// $this->dumpop($op, $EX); //var_dump($op);

			$resvar = null;
			if ((ZEND_ENGINE_2_4 ? ($res['op_type'] & EXT_TYPE_UNUSED) : ($res['EA.type'] & EXT_TYPE_UNUSED)) || $res['op_type'] == XC_IS_UNUSED) {
				$istmpres = false;
			}
			else {
				$istmpres = true;
			}
			// }}}
			// echo $opname, "\n";

			$call = array(&$this, $opname);
			if (is_callable($call)) {
				$this->{$opname}($op, $EX);
			}
			else if (isset($this->binops[$opc])) { // {{{
				$op1val = $this->getOpVal($op1, $EX, false);
				$op2val = $this->getOpVal($op2, $EX, false);
				$rvalue = new Decompiler_Binop($this, $op1val, $opc, $op2val);
				$resvar = $rvalue;
				// }}}
			}
			else if (isset($this->unaryops[$opc])) { // {{{
				$op1val = $this->getOpVal($op1, $EX);
				$myop = $this->unaryops[$opc];
				$rvalue = "$myop$op1val";
				$resvar = $rvalue;
				// }}}
			}
			else {
				switch ($opc) {
				case XC_NEW: // {{{
					array_push($EX['arg_types_stack'], array($EX['fbc'], $EX['object'], $EX['called_scope']));
					$EX['object'] = (int) $res['var'];
					$EX['called_scope'] = null;
					$EX['fbc'] = 'new ' . $this->unquoteName($this->getOpVal($op1, $EX));
					if (!ZEND_ENGINE_2) {
						$resvar = '$new object$';
					}
					break;
					// }}}
				case XC_FETCH_CLASS: // {{{
					if ($op2['op_type'] == XC_IS_UNUSED) {
						switch (($ext & (defined('ZEND_FETCH_CLASS_MASK') ? ZEND_FETCH_CLASS_MASK : 0xFF))) {
						case ZEND_FETCH_CLASS_SELF:
							$class = 'self';
							break;
						case ZEND_FETCH_CLASS_PARENT:
							$class = 'parent';
							break;
						case ZEND_FETCH_CLASS_STATIC:
							$class = 'static';
							break;
						}
						$istmpres = true;
					}
					else {
						$class = $op2['constant'];
						if (is_object($class)) {
							$class = get_class($class);
						}
					}
					$resvar = $class;
					break;
					// }}}
				case XC_FETCH_CONSTANT: // {{{
					if ($op1['op_type'] == XC_IS_CONST) {
						$resvar = $op1['constant'];
					}
					else if ($op1['op_type'] == XC_IS_UNUSED) {
						$resvar = $op2['constant'];
					}
					else {
						$class = $T[$op1['var']];
						assert($class[0] == 'class');
						$resvar = $class[1] . '::' . $op2['constant'];
					}
					break;
					// }}}
					// {{{ case XC_FETCH_*
				case XC_FETCH_R:
				case XC_FETCH_W:
				case XC_FETCH_RW:
				case XC_FETCH_FUNC_ARG:
				case XC_FETCH_UNSET:
				case XC_FETCH_IS:
				case XC_UNSET_VAR:
					$rvalue = $this->getOpVal($op1, $EX);
					if (defined('ZEND_FETCH_TYPE_MASK')) {
						$fetchtype = ($ext & ZEND_FETCH_TYPE_MASK);
					}
					else {
						$fetchtype = $op2[!ZEND_ENGINE_2 ? 'fetch_type' : 'EA.type'];
					}
					switch ($fetchtype) {
					case ZEND_FETCH_STATIC_MEMBER:
						$class = $this->getOpVal($op2, $EX);
						$rvalue = $class . '::$' . $this->unquoteName($rvalue);
						break;
					default:
						$name = $this->unquoteName($rvalue);
						$globalname = xcache_is_autoglobal($name) ? "\$$name" : "\$GLOBALS[$rvalue]";
						$rvalue = new Decompiler_Fetch($rvalue, $fetchtype, $globalname);
						break;
					}
					if ($opc == XC_UNSET_VAR) {
						$op['php'] = "unset(" . toCode($rvalue, $EX) . ")";
						$lastphpop = &$op;
					}
					else if ($res['op_type'] != XC_IS_UNUSED) {
						$resvar = $rvalue;
					}
					break;
					// }}}
					// {{{ case XC_FETCH_DIM_*
				case XC_FETCH_DIM_TMP_VAR:
				case XC_FETCH_DIM_R:
				case XC_FETCH_DIM_W:
				case XC_FETCH_DIM_RW:
				case XC_FETCH_DIM_FUNC_ARG:
				case XC_FETCH_DIM_UNSET:
				case XC_FETCH_DIM_IS:
				case XC_ASSIGN_DIM:
				case XC_UNSET_DIM:
				case XC_UNSET_DIM_OBJ:
					$src = $this->getOpVal($op1, $EX, false);
					if (is_a($src, "Decompiler_ForeachBox")) {
						$src->iskey = $this->getOpVal($op2, $EX);
						$resvar = $src;
						break;
					}
					else if (is_a($src, "Decompiler_DimBox")) {
						$dimbox = $src;
					}
					else {
						if (!is_a($src, "Decompiler_ListBox")) {
							$list = new Decompiler_List($this->getOpVal($op1, $EX, false));

							$src = new Decompiler_ListBox($list);
							if (!isset($op1['var'])) {
								$this->dumpop($op, $EX);
								var_dump($op);
								die('missing var');
							}
							$T[$op1['var']] = $src;
							unset($list);
						}
						$dim = new Decompiler_Dim($src);
						$src->obj->dims[] = &$dim;

						$dimbox = new Decompiler_DimBox($dim);
					}
					$dim = &$dimbox->obj;
					$dim->offsets[] = $this->getOpVal($op2, $EX);
					if ($ext == ZEND_FETCH_ADD_LOCK) {
						$src->obj->everLocked = true;
					}
					else if ($ext == ZEND_FETCH_STANDARD) {
						$dim->isLast = true;
					}
					unset($dim);
					$rvalue = $dimbox;

					if ($opc == XC_ASSIGN_DIM) {
						$lvalue = $rvalue;
						++ $i;
						$rvalue = $this->getOpVal($opcodes[$i]['op1'], $EX);
						$resvar = toCode($lvalue, $EX) . ' = ' . $rvalue;
					}
					else if ($opc == XC_UNSET_DIM) {
						$op['php'] = "unset(" . toCode($rvalue, $EX) . ")";
						$lastphpop = &$op;
					}
					else if ($res['op_type'] != XC_IS_UNUSED) {
						$resvar = $rvalue;
					}
					break;
					// }}}
				case XC_ASSIGN: // {{{
					$lvalue = $this->getOpVal($op1, $EX);
					$rvalue = $this->getOpVal($op2, $EX, false);
					if (is_a($rvalue, 'Decompiler_ForeachBox')) {
						$type = $rvalue->iskey ? 'fe_key' : 'fe_as';
						$rvalue->obj[$type] = $lvalue;
						unset($T[$op2['var']]);
						break;
					}
					if (is_a($rvalue, "Decompiler_DimBox")) {
						$dim = &$rvalue->obj;
						$dim->assign = $lvalue;
						if ($dim->isLast) {
							$resvar = toCode($dim->value, $EX);
						}
						unset($dim);
						break;
					}
					$resvar = "$lvalue = " . toCode($rvalue, $EX);
					break;
					// }}}
				case XC_ASSIGN_REF: // {{{
					$lvalue = $this->getOpVal($op1, $EX);
					$rvalue = $this->getOpVal($op2, $EX, false);
					if (is_a($rvalue, 'Decompiler_Fetch')) {
						$src = toCode($rvalue->src, $EX);
						if (substr($src, 1, -1) == substr($lvalue, 1)) {
							switch ($rvalue->fetchType) {
							case ZEND_FETCH_GLOBAL:
							case ZEND_FETCH_GLOBAL_LOCK:
								$resvar = 'global ' . $lvalue;
								break 2;
							case ZEND_FETCH_STATIC:
								$statics = &$EX['op_array']['static_variables'];
								$resvar = 'static ' . $lvalue;
								$name = substr($src, 1, -1);
								if (isset($statics[$name])) {
									$var = $statics[$name];
									$resvar .= ' = ';
									$resvar .= toCode(value($var), $EX);
								}
								unset($statics);
								break 2;
							default:
							}
						}
					}
					// TODO: PHP_6 global
					$rvalue = toCode($rvalue, $EX);
					$resvar = "$lvalue = &$rvalue";
					break;
					// }}}
				// {{{ case XC_FETCH_OBJ_*
				case XC_FETCH_OBJ_R:
				case XC_FETCH_OBJ_W:
				case XC_FETCH_OBJ_RW:
				case XC_FETCH_OBJ_FUNC_ARG:
				case XC_FETCH_OBJ_UNSET:
				case XC_FETCH_OBJ_IS:
				case XC_ASSIGN_OBJ:
					$obj = $this->getOpVal($op1, $EX);
					if (!isset($obj)) {
						$obj = '$this';
					}
					$prop = $this->getOpVal($op2, $EX);
					if (preg_match($this->rQuotedName, $prop)) {
						$prop = substr($prop, 1, -1);;
						$rvalue = "{$obj}->$prop";
					}
					else {
						$rvalue = "{$obj}->{" . "$prop}";
					}
					if ($res['op_type'] != XC_IS_UNUSED) {
						$resvar = $rvalue;
					}
					if ($opc == XC_ASSIGN_OBJ) {
						++ $i;
						$lvalue = $rvalue;
						$rvalue = $this->getOpVal($opcodes[$i]['op1'], $EX);
						$resvar = "$lvalue = $rvalue";
					}
					break;
					// }}}
				case XC_ISSET_ISEMPTY_DIM_OBJ:
				case XC_ISSET_ISEMPTY_PROP_OBJ:
				case XC_ISSET_ISEMPTY:
				case XC_ISSET_ISEMPTY_VAR: // {{{
					if ($opc == XC_ISSET_ISEMPTY_VAR) {
						$rvalue = $this->getOpVal($op1, $EX);;
						if (preg_match($this->rQuotedName, $rvalue)) {
							$rvalue = '$' . substr($rvalue, 1, -1);
						}
						else {
							$rvalue = '${' . $rvalue . '}';
						}
						if ($op2['EA.type'] == ZEND_FETCH_STATIC_MEMBER) {
							$class = $this->getOpVal($op2, $EX);
							$rvalue = $class . '::' . $rvalue;
						}
					}
					else if ($opc == XC_ISSET_ISEMPTY) {
						$rvalue = $this->getOpVal($op1, $EX);
					}
					else {
						$container = $this->getOpVal($op1, $EX);
						$dim = $this->getOpVal($op2, $EX);
						if ($opc == XC_ISSET_ISEMPTY_PROP_OBJ) {
							if (preg_match($this->rQuotedName, $dim)) {
								$rvalue = $container . "->" . substr($dim, 1, -1);
							}
							else {
								$rvalue = $container . "->{" . $dim . "}";
							}
						}
						else {
							$rvalue = $container . "[$dim]";
						}
					}

					switch ((!ZEND_ENGINE_2 ? $op['op2']['var'] /* constant */ : $ext) & (ZEND_ISSET|ZEND_ISEMPTY)) {
					case ZEND_ISSET:
						$rvalue = "isset($rvalue)";
						break;
					case ZEND_ISEMPTY:
						$rvalue = "empty($rvalue)";
						break;
					}
					$resvar = $rvalue;
					break;
					// }}}
				case XC_SEND_VAR_NO_REF:
				case XC_SEND_VAL:
				case XC_SEND_REF:
				case XC_SEND_VAR: // {{{
					$ref = ($opc == XC_SEND_REF ? '&' : '');
					$EX['argstack'][] = $ref . $this->getOpVal($op1, $EX);
					break;
					// }}}
				case XC_INIT_STATIC_METHOD_CALL:
				case XC_INIT_METHOD_CALL:
				case XC_INIT_FCALL_BY_FUNC:
				case XC_INIT_FCALL_BY_NAME: // {{{
					if (($ext & ZEND_CTOR_CALL)) {
						break;
					}
					array_push($EX['arg_types_stack'], array($EX['fbc'], $EX['object'], $EX['called_scope']));
					if ($opc == XC_INIT_STATIC_METHOD_CALL || $opc == XC_INIT_METHOD_CALL || $op1['op_type'] != XC_IS_UNUSED) {
						$obj = $this->getOpVal($op1, $EX);
						if (!isset($obj)) {
							$obj = '$this';
						}
						if ($opc == XC_INIT_STATIC_METHOD_CALL || /* PHP4 */ isset($op1['constant'])) {
							$EX['object'] = null;
							$EX['called_scope'] = $this->unquoteName($obj);
						}
						else {
							$EX['object'] = $obj;
							$EX['called_scope'] = null;
						}
						if ($res['op_type'] != XC_IS_UNUSED) {
							$resvar = '$obj call$';
						}
					}
					else {
						$EX['object'] = null;
						$EX['called_scope'] = null;
					}

					if ($opc == XC_INIT_FCALL_BY_FUNC) {
						$which = $op1['var'];
						$EX['fbc'] = $EX['op_array']['funcs'][$which]['name'];
					}
					else {
						$EX['fbc'] = $this->getOpVal($op2, $EX, false);
					}
					break;
					// }}}
				case XC_DO_FCALL_BY_FUNC:
					$which = $op1['var'];
					$fname = $EX['op_array']['funcs'][$which]['name'];
					$args = $this->popargs($EX, $ext);
					$resvar = $fname . "($args)";
					break;
				case XC_DO_FCALL:
					$fname = $this->unquoteName($this->getOpVal($op1, $EX, false));
					$args = $this->popargs($EX, $ext);
					$resvar = $fname . "($args)";
					break;
				case XC_DO_FCALL_BY_NAME: // {{{
					$object = null;

					$fname = $this->unquoteName($EX['fbc']);
					if (!is_int($EX['object'])) {
						$object = $EX['object'];
					}

					$args = $this->popargs($EX, $ext);

					$resvar =
						(isset($object) ? $object . '->' : '' )
						. (isset($EX['called_scope']) ? $EX['called_scope'] . '::' : '' )
						. $fname . "($args)";
					unset($args);

					if (is_int($EX['object'])) {
						$T[$EX['object']] = $resvar;
						$resvar = null;
					}
					list($EX['fbc'], $EX['object'], $EX['called_scope']) = array_pop($EX['arg_types_stack']);
					break;
					// }}}
				case XC_VERIFY_ABSTRACT_CLASS: // {{{
					//unset($T[$op1['var']]);
					break;
					// }}}
				case XC_DECLARE_CLASS: 
				case XC_DECLARE_INHERITED_CLASS:
				case XC_DECLARE_INHERITED_CLASS_DELAYED: // {{{
					$key = $op1['constant'];
					if (!isset($this->dc['class_table'][$key])) {
						echo 'class not found: ', $key, 'existing classes are:', "\n";
						var_dump(array_keys($this->dc['class_table']));
						exit;
					}
					$class = &$this->dc['class_table'][$key];
					$class['name'] = $this->unquoteName($this->getOpVal($op2, $EX));
					if ($opc == XC_DECLARE_INHERITED_CLASS || $opc == XC_DECLARE_INHERITED_CLASS_DELAYED) {
						$ext /= XC_SIZEOF_TEMP_VARIABLE;
						$class['parent'] = $T[$ext];
						unset($T[$ext]);
					}
					else {
						$class['parent'] = null;
					}

					while ($i + 2 < $ic
					 && $opcodes[$i + 2]['opcode'] == XC_ADD_INTERFACE
					 && $opcodes[$i + 2]['op1']['var'] == $res['var']
					 && $opcodes[$i + 1]['opcode'] == XC_FETCH_CLASS) {
						$fetchop = &$opcodes[$i + 1];
						$impl = $this->unquoteName($this->getOpVal($fetchop['op2'], $EX));
						$addop = &$opcodes[$i + 2];
						$class['interfaces'][$addop['extended_value']] = $impl;
						unset($fetchop, $addop);
						$i += 2;
					}
					$this->dclass($class);
					unset($class);
					break;
					// }}}
				case XC_INIT_STRING: // {{{
					$resvar = "''";
					break;
					// }}}
				case XC_ADD_CHAR:
				case XC_ADD_STRING:
				case XC_ADD_VAR: // {{{
					$op1val = $this->getOpVal($op1, $EX);
					$op2val = $this->getOpVal($op2, $EX);
					switch ($opc) {
					case XC_ADD_CHAR:
						$op2val = toCode(chr($op2val), $EX);
						break;
					case XC_ADD_STRING:
						$op2val = toCode($op2val, $EX);
						break;
					case XC_ADD_VAR:
						break;
					}
					if ($op1val == "''") {
						$rvalue = $op2val;
					}
					else if ($op2val == "''") {
						$rvalue = $op1val;
					}
					else {
						$rvalue = $op1val . ' . ' . $op2val;
					}
					$resvar = $rvalue;
					// }}}
					break;
				case XC_PRINT: // {{{
					$op1val = $this->getOpVal($op1, $EX);
					$resvar = "print($op1val)";
					break;
					// }}}
				case XC_ECHO: // {{{
					$op1val = $this->getOpVal($op1, $EX);
					$resvar = "echo $op1val";
					break;
					// }}}
				case XC_EXIT: // {{{
					$op1val = $this->getOpVal($op1, $EX);
					$resvar = "exit($op1val)";
					break;
					// }}}
				case XC_INIT_ARRAY:
				case XC_ADD_ARRAY_ELEMENT: // {{{
					$rvalue = $this->getOpVal($op1, $EX, false, true);

					if ($opc == XC_ADD_ARRAY_ELEMENT) {
						$assoc = $this->getOpVal($op2, $EX);
						if (isset($assoc)) {
							$T[$res['var']]->value[] = array($assoc, $rvalue);
						}
						else {
							$T[$res['var']]->value[] = array(null, $rvalue);
						}
					}
					else {
						if ($opc == XC_INIT_ARRAY) {
							$resvar = new Decompiler_Array();
							if (!isset($rvalue)) {
								continue;
							}
						}

						$assoc = $this->getOpVal($op2, $EX);
						if (isset($assoc)) {
							$resvar->value[] = array($assoc, $rvalue);
						}
						else {
							$resvar->value[] = array(null, $rvalue);
						}
					}
					break;
					// }}}
				case XC_QM_ASSIGN: // {{{
					$resvar = $this->getOpVal($op1, $EX);
					break;
					// }}}
				case XC_BOOL: // {{{
					$resvar = /*'(bool) ' .*/ $this->getOpVal($op1, $EX);
					break;
					// }}}
				case XC_RETURN: // {{{
					$resvar = "return " . $this->getOpVal($op1, $EX);
					break;
					// }}}
				case XC_INCLUDE_OR_EVAL: // {{{
					$type = $op2['var']; // hack
					$keyword = $this->includeTypes[$type];
					$resvar = "$keyword(" . $this->getOpVal($op1, $EX) . ")";
					break;
					// }}}
				case XC_FE_RESET: // {{{
					$resvar = $this->getOpVal($op1, $EX);
					break;
					// }}}
				case XC_FE_FETCH: // {{{
					$op['fe_src'] = $this->getOpVal($op1, $EX);
					$fe = new Decompiler_ForeachBox($op);
					$fe->iskey = false;
					$T[$res['var']] = $fe;

					++ $i;
					if (($ext & ZEND_FE_FETCH_WITH_KEY)) {
						$fe = new Decompiler_ForeachBox($op);
						$fe->iskey = true;

						$res = $opcodes[$i]['result'];
						$T[$res['var']] = $fe;
					}
					break;
					// }}}
				case XC_SWITCH_FREE: // {{{
					// unset($T[$op1['var']]);
					break;
					// }}}
				case XC_FREE: // {{{
					$free = $T[$op1['var']];
					if (!is_a($free, 'Decompiler_Array') && !is_a($free, 'Decompiler_Box')) {
						$op['php'] = is_object($free) ? $free : $this->unquote($free, '(', ')');
						$lastphpop = &$op;
					}
					unset($T[$op1['var']], $free);
					break;
					// }}}
				case XC_JMP_NO_CTOR:
					break;
				case XC_JMPNZ: // while
				case XC_JMPZNZ: // for
				case XC_JMPZ_EX: // and
				case XC_JMPNZ_EX: // or
				case XC_JMPZ: // {{{
					if ($opc == XC_JMP_NO_CTOR && $EX['object']) {
						$rvalue = $EX['object'];
					}
					else {
						$rvalue = $this->getOpVal($op1, $EX);
					}

					if (isset($op['cond_true'])) {
						// any true comes here, so it's a "or"
						$rvalue = implode(' or ', $op['cond_true']) . ' or ' . $rvalue;
						unset($op['cond_true']);
					}
					if (isset($op['cond_false'])) {
						echo "TODO(cond_false):\n";
						var_dump($op);// exit;
					}
					if ($opc == XC_JMPZ_EX || $opc == XC_JMPNZ_EX || $opc == XC_JMPZ) {
						$targetop = &$EX['opcodes'][$op2['opline_num']];
						if ($opc == XC_JMPNZ_EX) {
							$targetop['cond_true'][] = toCode($rvalue, $EX);
						}
						else {
							$targetop['cond_false'][] = toCode($rvalue, $EX);
						}
						unset($targetop);
					}
					else {
						$op['cond'] = $rvalue; 
						$op['isjmp'] = true;
					}
					break;
					// }}}
				case XC_JMP: // {{{
					$op['cond'] = null;
					$op['isjmp'] = true;
					break;
					// }}}
				case XC_CASE:
				case XC_BRK:
					break;
				case XC_RECV_INIT:
				case XC_RECV:
					$offset = $this->getOpVal($op1, $EX);
					$lvalue = $this->getOpVal($op['result'], $EX);
					if ($opc == XC_RECV_INIT) {
						$default = value($op['op2']['constant']);
					}
					else {
						$default = null;
					}
					$EX['recvs'][str($offset)] = array($lvalue, $default);
					break;
				case XC_POST_DEC:
				case XC_POST_INC:
				case XC_POST_DEC_OBJ:
				case XC_POST_INC_OBJ:
				case XC_PRE_DEC:
				case XC_PRE_INC:
				case XC_PRE_DEC_OBJ:
				case XC_PRE_INC_OBJ: // {{{
					$flags = array_flip(explode('_', $opname));
					if (isset($flags['OBJ'])) {
						$resvar = $this->getOpVal($op1, $EX);
						$prop = $this->unquoteName($this->getOpVal($op2, $EX));
						if ($prop{0} == '$') {
							$resvar = $resvar . "{" . $prop . "}";
						}
						else {
							$resvar = $resvar . "->" . $prop;
						}
					}
					else {
						$resvar = $this->getOpVal($op1, $EX);
					}
					$opstr = isset($flags['DEC']) ? '--' : '++';
					if (isset($flags['POST'])) {
						$resvar .= ' ' . $opstr;
					}
					else {
						$resvar = "$opstr $resvar";
					}
					break;
					// }}}

				case XC_BEGIN_SILENCE: // {{{
					$EX['silence'] ++;
					break;
					// }}}
				case XC_END_SILENCE: // {{{
					$EX['silence'] --;
					$lastresvar = '@' . toCode($lastresvar, $EX);
					break;
					// }}}
				case XC_CONT: // {{{
					break;
					// }}}
				case XC_CAST: // {{{
					$type = $ext;
					static $type2cast = array(
							IS_LONG   => '(int)',
							IS_DOUBLE => '(double)',
							IS_STRING => '(string)',
							IS_ARRAY  => '(array)',
							IS_OBJECT => '(object)',
							IS_BOOL   => '(bool)',
							IS_NULL   => '(unset)',
							);
					assert(isset($type2cast[$type]));
					$cast = $type2cast[$type];
					$resvar = $cast . ' ' . $this->getOpVal($op1, $EX);
					break;
					// }}}
				case XC_EXT_STMT:
				case XC_EXT_FCALL_BEGIN:
				case XC_EXT_FCALL_END:
				case XC_EXT_NOP:
					break;
				case XC_DECLARE_FUNCTION_OR_CLASS:
					/* always removed by compiler */
					break;
				case XC_TICKS:
					$lastphpop['ticks'] = $this->getOpVal($op1, $EX);
					// $EX['tickschanged'] = true;
					break;
				default: // {{{
					echo "\x1B[31m * TODO ", $opname, "\x1B[0m\n";
					// }}}
				}
			}
			if (isset($resvar)) {
				if ($istmpres) {
					$T[$res['var']] = $resvar;
					$lastresvar = &$T[$res['var']];
				}
				else {
					$op['php'] = $resvar;
					$lastphpop = &$op;
					$lastresvar = &$op['php'];
				}
			}
		}
		return $T;
	}
	// }}}
	function unquote($str, $st, $ed) // {{{
	{
		$l1 = strlen($st);
		$l2 = strlen($ed);
		if (substr($str, 0, $l1) === $st && substr($str, -$l2) === $ed) {
			$str = substr($str, $l1, -$l2);
		}
		return $str;
	}
	// }}}
	function popargs(&$EX, $n) // {{{
	{
		$args = array();
		for ($i = 0; $i < $n; $i ++) {
			$a = array_pop($EX['argstack']);
			if (is_array($a)) {
				array_unshift($args, toCode($a, $EX));
			}
			else {
				array_unshift($args, $a);
			}
		}
		return implode(', ', $args);
	}
	// }}}
	function dumpop($op, &$EX) // {{{
	{
		$op1 = $op['op1'];
		$op2 = $op['op2'];
		$d = array('opname' => xcache_get_opcode($op['opcode']), 'opcode' => $op['opcode']);

		foreach (array('op1' => 'op1', 'op2' => 'op2', 'result' => 'res') as $k => $kk) {
			switch ($op[$k]['op_type']) {
			case XC_IS_UNUSED:
				$d[$kk] = '*UNUSED* ' . $op[$k]['opline_num'];
				break;

			case XC_IS_VAR:
				$d[$kk] = '$' . $op[$k]['var'];
				if ($kk != 'res') {
					$d[$kk] .= ':' . $this->getOpVal($op[$k], $EX);
				}
				break;

			case XC_IS_TMP_VAR:
				$d[$kk] = '#' . $op[$k]['var'];
				if ($kk != 'res') {
					$d[$kk] .= ':' . $this->getOpVal($op[$k], $EX);
				}
				break;

			case XC_IS_CV:
				$d[$kk] = $this->getOpVal($op[$k], $EX);
				break;

			default:
				if ($kk == 'res') {
					var_dump($op);
					exit;
					assert(0);
				}
				else {
					$d[$kk] = $this->getOpVal($op[$k], $EX);
				}
			}
		}
		$d['ext'] = $op['extended_value'];

		var_dump($d);
	}
	// }}}
	function dargs(&$EX, $indent) // {{{
	{
		$op_array = &$EX['op_array'];

		if (isset($op_array['num_args'])) {
			$c = $op_array['num_args'];
		}
		else if ($op_array['arg_types']) {
			$c = count($op_array['arg_types']);
		}
		else {
			// php4
			$c = count($EX['recvs']);
		}

		$refrest = false;
		for ($i = 0; $i < $c; $i ++) {
			if ($i) {
				echo ', ';
			}
			$arg = $EX['recvs'][$i + 1];
			if (isset($op_array['arg_info'])) {
				$ai = $op_array['arg_info'][$i];
				if (!empty($ai['class_name'])) {
					echo $ai['class_name'], ' ';
					if (!ZEND_ENGINE_2_2 && $ai['allow_null']) {
						echo 'or NULL ';
					}
				}
				else if (!empty($ai['array_type_hint'])) {
					echo 'array ';
					if (!ZEND_ENGINE_2_2 && $ai['allow_null']) {
						echo 'or NULL ';
					}
				}
				if ($ai['pass_by_reference']) {
					echo '&';
				}
				printf("\$%s", $ai['name']);
			}
			else {
				if ($refrest) {
					echo '&';
				}
				else if (isset($op_array['arg_types'][$i])) {
					switch ($op_array['arg_types'][$i]) {
					case BYREF_FORCE_REST:
						$refrest = true;
						/* fall */
					case BYREF_FORCE:
						echo '&';
						break;

					case BYREF_NONE:
					case BYREF_ALLOW:
						break;
					default:
						assert(0);
					}
				}
				echo toCode($arg[0], $indent);
			}
			if (isset($arg[1])) {
				echo ' = ', toCode($arg[1], $indent);
			}
		}
	}
	// }}}
	function dfunction($func, $indent = '', $nobody = false) // {{{
	{
		if ($nobody) {
			$body = ";\n";
			$EX = array();
			$EX['op_array'] = &$func['op_array'];
			$EX['recvs'] = array();
		}
		else {
			ob_start();
			$newindent = INDENT . $indent;
			$EX = &$this->dop_array($func['op_array'], $newindent);
			$body = ob_get_clean();
			if (!isset($EX['recvs'])) {
				$EX['recvs'] = array();
			}
		}

		echo 'function ', $func['op_array']['function_name'], '(';
		$this->dargs($EX, $indent);
		echo ")\n";
		echo $indent, "{\n";
		echo $body;
		echo "$indent}\n";
	}
	// }}}
	function dclass($class, $indent = '') // {{{
	{
		// {{{ class decl
		if (!empty($class['doc_comment'])) {
			echo $indent;
			echo $class['doc_comment'];
			echo "\n";
		}
		$isinterface = false;
		if (!empty($class['ce_flags'])) {
			if ($class['ce_flags'] & ZEND_ACC_INTERFACE) {
				echo 'interface ';
				$isinterface = true;
			}
			else {
				if ($class['ce_flags'] & ZEND_ACC_IMPLICIT_ABSTRACT) {
					echo "abstract ";
				}
				if ($class['ce_flags'] & ZEND_ACC_FINAL) {
					echo "final ";
				}
			}
		}
		echo 'class ', $class['name'];
		if ($class['parent']) {
			echo ' extends ', $class['parent'];
		}
		/* TODO */
		if (!empty($class['interfaces'])) {
			echo ' implements ';
			echo implode(', ', $class['interfaces']);
		}
		echo "\n";
		echo $indent, "{";
		// }}}
		$newindent = INDENT . $indent;
		// {{{ const, static
		foreach (array('constants_table' => 'const '
					, 'static_members' => 'static $') as $type => $prefix) {
			if (!empty($class[$type])) {
				echo "\n";
				// TODO: skip shadow?
				foreach ($class[$type] as $name => $v) {
					echo $newindent;
					echo $prefix, $name, ' = ';
					echo toCode(value($v), $newindent);
					echo ";\n";
				}
			}
		}
		// }}}
		// {{{ properties
		$member_variables = isset($class['properties_info']) ? $class['properties_info'] : ($class['default_static_members'] + $class['default_properties']);
		if ($member_variables) {
			echo "\n";
			$infos = !empty($class['properties_info']) ? $class['properties_info'] : null;
			foreach ($member_variables as $name => $dummy) {
				$info = (isset($infos) && isset($infos[$name])) ? $infos[$name] : null;
				if (isset($info)) {
					if (!empty($info['doc_comment'])) {
						echo $newindent;
						echo $info['doc_comment'];
						echo "\n";
					}
				}

				echo $newindent;
				$static = false;
				if (isset($info)) {
					if ($info['flags'] & ZEND_ACC_STATIC) {
						$static = true;
					}
				}
				else if (isset($class['default_static_members'][$name])) {
					$static = true;
				}

				if ($static) {
					echo "static ";
				}

				$mangled = false;
				if (!ZEND_ENGINE_2) {
					echo 'var ';
				}
				else if (!isset($info)) {
					echo 'public ';
				}
				else {
					if ($info['flags'] & ZEND_ACC_SHADOW) {
						continue;
					}
					switch ($info['flags'] & ZEND_ACC_PPP_MASK) {
					case ZEND_ACC_PUBLIC:
						echo "public ";
						break;
					case ZEND_ACC_PRIVATE:
						echo "private ";
						$mangled = true;
						break;
					case ZEND_ACC_PROTECTED:
						echo "protected ";
						$mangled = true;
						break;
					}
				}

				echo '$', $name;

				if (isset($info['offset'])) {
					$value = $class[$static ? 'default_static_members_table' : 'default_properties_table'][$info['offset']];
				}
				else {
					$key = isset($info) ? $info['name'] . ($mangled ? "\000" : "") : $name;

					$value = $class[$static ? 'default_static_members' : 'default_properties'][$key];
				}
				if (isset($value)) {
					echo ' = ';
					echo toCode(value($value), $newindent);
				}
				echo ";\n";
			}
		}
		// }}}
		// {{{ function_table
		if (isset($class['function_table'])) {
			foreach ($class['function_table'] as $func) {
				if (!isset($func['scope']) || $func['scope'] == $class['name']) {
					// TODO: skip shadow here
					echo "\n";
					$opa = $func['op_array'];
					if (!empty($opa['doc_comment'])) {
						echo $newindent;
						echo $opa['doc_comment'];
						echo "\n";
					}
					echo $newindent;
					if (isset($opa['fn_flags'])) {
						if ($opa['fn_flags'] & ZEND_ACC_ABSTRACT) {
							echo "abstract ";
						}
						if ($opa['fn_flags'] & ZEND_ACC_FINAL) {
							echo "final ";
						}
						if ($opa['fn_flags'] & ZEND_ACC_STATIC) {
							echo "static ";
						}

						switch ($opa['fn_flags'] & ZEND_ACC_PPP_MASK) {
							case ZEND_ACC_PUBLIC:
								echo "public ";
								break;
							case ZEND_ACC_PRIVATE:
								echo "private ";
								break;
							case ZEND_ACC_PROTECTED:
								echo "protected ";
								break;
							default:
								echo "<visibility error> ";
								break;
						}
					}
					$this->dfunction($func, $newindent, $isinterface);
					if ($opa['function_name'] == 'Decompiler') {
						//exit;
					}
				}
			}
		}
		// }}}
		echo $indent, "}\n";
	}
	// }}}
	function decompileString($string) // {{{
	{
		$this->dc = xcache_dasm_string($string);
		if ($this->dc === false) {
			echo "error compling string\n";
			return false;
		}
	}
	// }}}
	function decompileFile($file) // {{{
	{
		$this->dc = xcache_dasm_file($file);
		if ($this->dc === false) {
			echo "error compling $file\n";
			return false;
		}
	}
	// }}}
	function output() // {{{
	{
		echo "<?". "php\n";
		foreach ($this->dc['class_table'] as $key => $class) {
			if ($key{0} != "\0") {
				echo "\n";
				$this->dclass($class);
			}
		}

		foreach ($this->dc['function_table'] as $key => $func) {
			if ($key{0} != "\0") {
				echo "\n";
				$this->dfunction($func);
			}
		}

		echo "\n";
		$this->dop_array($this->dc['op_array']);
		echo "\n?" . ">\n";
		return true;
	}
	// }}}
}

// {{{ defines
define('ZEND_ACC_STATIC',         0x01);
define('ZEND_ACC_ABSTRACT',       0x02);
define('ZEND_ACC_FINAL',          0x04);
define('ZEND_ACC_IMPLEMENTED_ABSTRACT',       0x08);

define('ZEND_ACC_IMPLICIT_ABSTRACT_CLASS',    0x10);
define('ZEND_ACC_EXPLICIT_ABSTRACT_CLASS',    0x20);
define('ZEND_ACC_FINAL_CLASS',                0x40);
define('ZEND_ACC_INTERFACE',                  0x80);
define('ZEND_ACC_PUBLIC',     0x100);
define('ZEND_ACC_PROTECTED',  0x200);
define('ZEND_ACC_PRIVATE',    0x400);
define('ZEND_ACC_PPP_MASK',  (ZEND_ACC_PUBLIC | ZEND_ACC_PROTECTED | ZEND_ACC_PRIVATE));

define('ZEND_ACC_CHANGED',    0x800);
define('ZEND_ACC_IMPLICIT_PUBLIC',    0x1000);

define('ZEND_ACC_CTOR',       0x2000);
define('ZEND_ACC_DTOR',       0x4000);
define('ZEND_ACC_CLONE',      0x8000);

define('ZEND_ACC_ALLOW_STATIC',   0x10000);

define('ZEND_ACC_SHADOW', 0x2000);

define('ZEND_ENGINE_2_4', PHP_VERSION >= "5.3.99");
define('ZEND_ENGINE_2_3', ZEND_ENGINE_2_4 || PHP_VERSION >= "5.3.");
define('ZEND_ENGINE_2_2', ZEND_ENGINE_2_3 || PHP_VERSION >= "5.2.");
define('ZEND_ENGINE_2_1', ZEND_ENGINE_2_2 || PHP_VERSION >= "5.1.");
define('ZEND_ENGINE_2',   ZEND_ENGINE_2_1 || PHP_VERSION >= "5.0.");

if (ZEND_ENGINE_2_4) {
	define('ZEND_FETCH_GLOBAL',           0x00000000);
	define('ZEND_FETCH_LOCAL',            0x10000000);
	define('ZEND_FETCH_STATIC',           0x20000000);
	define('ZEND_FETCH_STATIC_MEMBER',    0x30000000);
	define('ZEND_FETCH_GLOBAL_LOCK',      0x40000000);
	define('ZEND_FETCH_LEXICAL',          0x50000000);

	define('ZEND_FETCH_TYPE_MASK',        0x70000000);
}
else {
	define('ZEND_FETCH_GLOBAL',           0);
	define('ZEND_FETCH_LOCAL',            1);
	define('ZEND_FETCH_STATIC',           2);
	define('ZEND_FETCH_STATIC_MEMBER',    3);
	define('ZEND_FETCH_GLOBAL_LOCK',      4);
}

define('ZEND_FETCH_CLASS_DEFAULT',    0);
define('ZEND_FETCH_CLASS_SELF',       1);
define('ZEND_FETCH_CLASS_PARENT',     2);
define('ZEND_FETCH_CLASS_MAIN',       3);
define('ZEND_FETCH_CLASS_GLOBAL',     4);
define('ZEND_FETCH_CLASS_AUTO',       5);
define('ZEND_FETCH_CLASS_INTERFACE',  6);
define('ZEND_FETCH_CLASS_STATIC',     7);
if (ZEND_ENGINE_2_4) {
	define('ZEND_FETCH_CLASS_TRAIT',     14);
}
if (ZEND_ENGINE_2_3) {
	define('ZEND_FETCH_CLASS_MASK',     0xF);
}

define('ZEND_EVAL',               (1<<0));
define('ZEND_INCLUDE',            (1<<1));
define('ZEND_INCLUDE_ONCE',       (1<<2));
define('ZEND_REQUIRE',            (1<<3));
define('ZEND_REQUIRE_ONCE',       (1<<4));

define('ZEND_ISSET',              (1<<0));
define('ZEND_ISEMPTY',            (1<<1));
if (ZEND_ENGINE_2_4) {
	define('EXT_TYPE_UNUSED',     (1<<5));
}
else {
	define('EXT_TYPE_UNUSED',     (1<<0));
}

define('ZEND_FETCH_STANDARD',     0);
define('ZEND_FETCH_ADD_LOCK',     1);

define('ZEND_FE_FETCH_BYREF',     1);
define('ZEND_FE_FETCH_WITH_KEY',  2);

define('ZEND_MEMBER_FUNC_CALL',   1<<0);
define('ZEND_CTOR_CALL',          1<<1);

define('ZEND_ARG_SEND_BY_REF',        (1<<0));
define('ZEND_ARG_COMPILE_TIME_BOUND', (1<<1));
define('ZEND_ARG_SEND_FUNCTION',      (1<<2));

define('BYREF_NONE',       0);
define('BYREF_FORCE',      1);
define('BYREF_ALLOW',      2);
define('BYREF_FORCE_REST', 3);
define('IS_NULL',     0);
define('IS_LONG',     1);
define('IS_DOUBLE',   2);
define('IS_STRING',   3);
define('IS_ARRAY',    4);
define('IS_OBJECT',   5);
define('IS_BOOL',     6);
define('IS_RESOURCE', 7);
define('IS_CONSTANT', 8);
define('IS_CONSTANT_ARRAY',   9);

@define('XC_IS_CV', 16);

/*
if (preg_match_all('!XC_[A-Z_]+!', file_get_contents(__FILE__), $ms)) {
	$verdiff = array();
	foreach ($ms[0] as $k) {
		if (!defined($k)) {
			$verdiff[$k] = -1;
			define($k, -1);
		}
	}
	var_export($verdiff);
}
/*/
foreach (array (
	'XC_HANDLE_EXCEPTION' => -1,
	'XC_FETCH_CLASS' => -1,
	'XC_FETCH_' => -1,
	'XC_FETCH_DIM_' => -1,
	'XC_ASSIGN_DIM' => -1,
	'XC_UNSET_DIM' => -1,
	'XC_FETCH_OBJ_' => -1,
	'XC_ASSIGN_OBJ' => -1,
	'XC_ISSET_ISEMPTY_DIM_OBJ' => -1,
	'XC_ISSET_ISEMPTY_PROP_OBJ' => -1,
	'XC_ISSET_ISEMPTY_VAR' => -1,
	'XC_INIT_STATIC_METHOD_CALL' => -1,
	'XC_INIT_METHOD_CALL' => -1,
	'XC_VERIFY_ABSTRACT_CLASS' => -1,
	'XC_DECLARE_CLASS' => -1,
	'XC_DECLARE_INHERITED_CLASS' => -1,
	'XC_DECLARE_INHERITED_CLASS_DELAYED' => -1,
	'XC_ADD_INTERFACE' => -1,
	'XC_POST_DEC_OBJ' => -1,
	'XC_POST_INC_OBJ' => -1,
	'XC_PRE_DEC_OBJ' => -1,
	'XC_PRE_INC_OBJ' => -1,
	'XC_UNSET_OBJ' => -1,
	'XC_JMP_NO_CTOR' => -1,
	'XC_FETCH_' => -1,
	'XC_FETCH_DIM_' => -1,
	'XC_UNSET_DIM_OBJ' => -1,
	'XC_FETCH_OBJ_' => -1,
	'XC_ISSET_ISEMPTY' => -1,
	'XC_INIT_FCALL_BY_FUNC' => -1,
	'XC_DO_FCALL_BY_FUNC' => -1,
	'XC_DECLARE_FUNCTION_OR_CLASS' => -1,
) as $k => $v) {
	if (!defined($k)) {
		define($k, $v);
	}
}

/* XC_UNDEF XC_OP_DATA
$content = file_get_contents(__FILE__);
for ($i = 0; $opname = xcache_get_opcode($i); $i ++) {
	if (!preg_match("/\\bXC_" . $opname . "\\b(?!')/", $content)) {
		echo "not done ", $opname, "\n";
	}
}
// */
// }}}

