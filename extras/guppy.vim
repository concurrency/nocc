" Vim syntax file
" Language:    guppy (general-purpose process-oriented programming)
" Copyright:   Fred Barnes <frmb@kent.ac.uk>
" Maintainer:  Fred Barnes <frmb@kent.ac.uk>
" Last Change: 06/12/2010

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

" some settings
setlocal shiftwidth=2
setlocal softtabstop=2
setlocal expandtab

syn case match

syn match guppyType		/\<int[0-9]*\>/
syn match guppyType		/\<uint[0-9]*\>/
syn match guppyType		/\<real[0-9]*\>/
syn match guppyType		/\<chan\>/
syn match guppyType		/\<timer\>/
syn keyword guppyType		bool byte char string shared barrier trunc round
syn match guppyOperator		/!\|?\|+\|-\|\/\|\\\|*\|&\||\|&&\|||\|<<\|>>/
syn match guppyOperator		/->\|!=\|<>/
syn match guppySpecialChar	/\M\\\\\|\\'\|\\"\|\\x\([0-9a-fA-F]\+\)/ contained
syn match guppyNumber		/\<\d\+\(\.\d\+\(E\(+\|-\)\d\+\)\=\)\=/
syn match guppyNumber		/-\d\+\(\.\d\+\(E\(+\|-\)\d\+\)\=\)\=/
syn match guppyNumber		/0x\(\d\|[a-fA-F]\)\+/
syn match guppyNumber		/-0x\(\d\|[a-fA-F]\)\+/
syn match guppyIdentifier	/\<[A-Z_][A-Za-z0-9_]*\>/
syn match guppyBrackets		/\[\|\]/
syn match guppyTuple		/{\|}/
syn match guppyParentheses	/(\|)/
syn match guppyChar		/'[^']*'/

syn region guppyString		start=/"/ skip=/\M\\"/ end=/"/ contains=guppySpecialChar

syn keyword guppyKeyword	while if skip stop throw catch finally do define par bind type def seq
syn keyword guppyKeyword	initial val protocol subprotocol case alt pri for return break
syn keyword guppyKeyword	continue default public foreach else null spawn end claim sync
syn keyword guppySpecialFcn	size protocolof typeof is as

syn keyword guppyNamedConst	true false

syn match guppyComment		/#.*/
syn match guppyPreProc		/@[a-zA-Z]*/


if version >= 508 || !exists("did_guppy_syn_inits")
  if version < 508
    let did_guppy_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink guppyType Type
  HiLink guppyKeyword Keyword
  HiLink guppyOperator Operator
  HiLink guppyComment Comment
  HiLink guppyString String
  HiLink guppyChar String
  HiLink guppyNamedConst String
  HiLink guppyNumber Number
  HiLink guppySpecialFcn Type
  HiLink guppyPreProc PreProc
  HiLink guppyIdentifier Identifier
  HiLink guppyBrackets Type
  HiLink guppyTuple Type
  HiLink guppyParentheses Delimiter

  delcommand HiLink
endif

let b:current_syntax = "mcsp"
