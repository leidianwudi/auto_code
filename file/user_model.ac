// User Model Template
// 生成用户模型的 TypeScript 接口和服务类

export interface ${modelName} {
${each fields}
  ${name}: ${type};
${/each}
}

${if hasService}
export class ${modelName}Service {
  static baseUrl = '/api/${modelName.toLowerCase}';

  static async getAll(): Promise<${modelName}[]> {
    const response = await fetch(this.baseUrl);
    return response.json();
  }

  static async getById(id: number): Promise<${modelName}> {
    const response = await fetch(`${this.baseUrl}/${id}`);
    return response.json();
  }
}
${/if}