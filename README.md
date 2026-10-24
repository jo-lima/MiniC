# MiniC

Compilador didático para um subconjunto de C, com implementações em Python e C. O projeto inclui análise léxica, análise sintática, verificação semântica e emissão opcional de uma representação intermediária (IR).

## Estrutura

- `MiniCompiler/MiniCompiler.py`: ponto de entrada Python; coordena scanner, parser, análise semântica e geração de IR.
- `MiniCompiler/scanner.py`: transforma o texto em tokens e registra erros léxicos.
- `MiniCompiler/MiniCompiler.c`: implementação C do compilador.
- `testes/testes-semanticos/`: fontes e gabaritos dos testes semânticos.
- `testes/ir-test-cases/`: fontes e critérios dos testes de IR e otimização.
- `testes/testes-ap1/TESTES-AP1-PT2/testes-parser-50/`: fixtures do parser, incluindo ASTs esperadas.

## Requisitos

- Python 3 para executar a versão Python.
- GCC e Bash para compilar a versão C e executar os runners `.sh` no Linux.

## Uso

### Python

```bash
python3 MiniCompiler/MiniCompiler.py programa.c
```

### C

Compile uma vez e execute o binário:

```bash
gcc -std=c11 -Wall -Wextra MiniCompiler/MiniCompiler.c -o minic
./minic programa.c
```

### Opções

```bash
python3 MiniCompiler/MiniCompiler.py --diagnostics programa.c
python3 MiniCompiler/MiniCompiler.py --ir programa.c
python3 MiniCompiler/MiniCompiler.py -O1 --ir programa.c
./minic --diagnostics programa.c
./minic --ir programa.c
./minic -O1 --ir programa.c
```

- Sem opções, o compilador valida o programa e informa se foi aceito.
- `--diagnostics` solicita a listagem dos erros léxicos e sintáticos encontrados.
- `--ir` emite a representação intermediária.
- `-O0` seleciona o modo sem otimização (padrão); `-O1` ativa as otimizações implementadas.

A CLI atual não imprime a AST para entradas válidas: imprime a mensagem de aceitação ou, com `--ir`, a IR. Por isso, os fixtures da suíte AP1 que esperam a AST não correspondem à saída atual da CLI.

## Testes

Execute os runners a partir da raiz do repositório.

### IR em Python

```bash
bash testes/ir-test-cases/test_ir_python.sh \
  MiniCompiler/MiniCompiler.py \
  testes/ir-test-cases/testes
```

### IR em C

```bash
bash testes/ir-test-cases/test_ir_c.sh \
  MiniCompiler/MiniCompiler.c \
  testes/ir-test-cases/testes
```

O runner C usa GCC por padrão; é possível escolher outro compilador compatível com `CC`, por exemplo `CC=clang`. Os runners de IR verificam aceitação/rejeição e emissão de IR; eles não comparam toda a IR gerada byte a byte com os gabaritos. Os critérios e limitações estão em `testes/ir-test-cases/README.md`.

### Testes semânticos

```bash
bash testes/testes-semanticos/testes_semanticos_py.sh \
  MiniCompiler/MiniCompiler.py \
  testes/testes-semanticos/testes

bash testes/testes-semanticos/testes_semanticos_c.sh \
  MiniCompiler/MiniCompiler.c \
  testes/testes-semanticos/testes
```

Esses scripts comparam a saída dos compiladores com os arquivos `.gabarito`.
