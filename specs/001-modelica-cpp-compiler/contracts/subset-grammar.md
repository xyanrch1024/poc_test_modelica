# 契约: 首期支持的 Modelica 子集文法 (EBNF)

范围依据 spec FR-004；文法之外的构造按 MC 系列错误拒绝（FR-005）。

```ebnf
file            = model_def , EOF ;

model_def       = "model" , IDENT , [ string_comment ] ,
                  { component } ,
                  "equation" , { equation } ,
                  [ experiment_annotation ] ,
                  "end" , IDENT , ";" ;

component       = ( "constant" | "parameter" | "Real" | "Integer" | "Boolean" ) ,
                  IDENT ,
                  { attribute } , [ "=" , expression ] , [ string_comment ] , ";" ;

attribute       = "(" , attr_item , { "," , attr_item } , ")" ;
attr_item       = ("start" | "fixed" | "unit") , "=" , (expression | string) ;

equation        = simple_eq ;
simple_eq       = expr_side , "=" , expr_side , ";" ;
expr_side       = expression | der_call ;
der_call        = "der" , "(" , IDENT , ")" ;

experiment_annotation
                = "annotation" , "(" , "experiment" , "(" ,
                  exp_item , { "," , exp_item } , ")" , ")" , ";" ;
exp_item        = ("StartTime" | "StopTime" | "Interval" | "Tolerance") ,
                  "=" , NUMBER ;

expression      = logical_or ;
logical_or      = logical_and , { "or" , logical_and } ;
logical_and     = not_expr , { "and" , not_expr } ;
not_expr        = [ "not" ] , comparison ;
comparison      = additive , [ ("<" | "<=" | ">" | ">=" | "==" | "<>") , additive ] ;
additive        = multiplicative , { ("+" | "-") , multiplicative } ;
multiplicative  = unary , { ("*" | "/") , unary } ;
unary           = [ "+" | "-" ] , primary ;
primary         = NUMBER | BOOLEAN | IDENT
                | IDENT , "(" , [ args ] , ")"
                | "(" , expression , ")" ;
args            = expression , { "," , expression } ;

string          = '"' , { any_char_except_quote } , '"' ;
string_comment  = string ;
NUMBER          = real_literal | integer_literal ;
BOOLEAN         = "true" | "false" ;
```

## 语义白名单

- 函数调用仅限：`abs sqrt sin cos tan exp log log10 min max`（MC0202）。
- `der(IDENT)` 仅允许出现在方程左侧（MC0203）。
- `Integer`/`Boolean` 变量不得作为状态量（无导数定义，MC0304 关联）。
- `unit` 属性首期解析但忽略其语义（spec Assumptions：不做单位检查）。

## 明确不支持（遇到即报错并指出位置）

`package`、`import`、`algorithm` 节、`when/if` 方程、`connect`、数组与向量、
`record`、`function` 定义、`each`、`reinit`、离散事件构造。
