${# ============================================================}
${# engine_sample.tpl — 模板引擎测试模板（test_engine_main.ac 调用）}
${# ------------------------------------------------------------}
${# 覆盖：变量替换 / 嵌套路径 / each 循环与元变量 / if-else /  }
${#       等值比较 / 算术表达式 / str 函数调用 / 引号内逗号参数 }
${# ============================================================}
name=${name}
count=${count}
url=${meta.dataUrl}
deep=${meta.nested.key}
${each item in items}${item_index}:${item.name};${/each}
${if flag}FLAG_ON${else}FLAG_OFF${/if}
${if empty}NONEMPTY${else}EMPTY${/if}
${if name == "AutoCode"}NAME_EQ${/if}
${if name != "other"}NAME_NE${/if}
sum=${a + b}
prod=${a * b + 2}
upper=${str.toUpperCase(name)}
repl=${str.replace(s, "a,b", "X")}
notexist=${fileExists("___no_such_file___.xyz")}
diagA=${str.replace(s, "a", "Z")}
diagB=${str.replace("a,b,c", "a,b", "X")}
