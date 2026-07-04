// Product List Template
// 生成商品列表页面代码

${if hasCategory}
// 分类: ${category}
${/if}

export interface ${itemName} {
${each fields}
  ${name}: ${type};
${/each}
}

// 分页参数
const pageSize = ${pageSize ? pageSize : 20};

export async function fetch${itemName}List(page: number = 1) {
  const response = await fetch('/api/${itemName.toLowerCase}/list', {
    method: 'GET',
    params: { page, pageSize }
  });
  return response.json();
}