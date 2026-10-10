"""Analisador léxico independente para a linguagem MiniC."""
from dataclasses import dataclass


@dataclass
class Token:
    kind: str
    text: str
    line: int
    column: int


@dataclass
class Diagnostic:
    message: str
    token: Token
    phase: str

    def __str__(self):
        prefix = "Erro léxico" if self.phase == "lexical" else "Erro sintático"
        return f"{prefix}: {self.message} (linha {self.token.line}, coluna {self.token.column})"


KEYWORDS = {
    "int", "float", "bool", "void", "return", "if", "else", "while",
    "true", "false",
}
TYPES = {"int", "float", "bool", "void"}


class Scanner:
    """Converte a entrada em tokens e coleta todos os erros lexicos."""

    def __init__(self, source):
        self.source = source
        self.errors = []

    def scan(self):
        tokens = []
        source_index = 0
        line_number = 1
        column_number = 1

        def update_position(consumed_text):
            nonlocal line_number, column_number
            if "\n" in consumed_text:
                line_number += consumed_text.count("\n")
                column_number = len(consumed_text.rsplit("\n", 1)[-1]) + 1
            else:
                column_number += len(consumed_text)

        while source_index < len(self.source):
            current_character = self.source[source_index]
            token_start = (line_number, column_number)

            if current_character.isspace():
                update_position(current_character)
                source_index += 1
                continue

            if self.source.startswith("//", source_index):
                end_index = self.source.find("\n", source_index)
                end_index = len(self.source) if end_index < 0 else end_index
                update_position(self.source[source_index:end_index])
                source_index = end_index
                continue

            if self.source.startswith("/*", source_index):
                end_index = self.source.find("*/", source_index + 2)
                if end_index < 0:
                    self.errors.append(Diagnostic(
                        "comentário não terminado",
                        Token("EOF", "", line_number, column_number),
                        "lexical",
                    ))
                    break
                end_index += 2
                update_position(self.source[source_index:end_index])
                source_index = end_index
                continue

            if current_character.isalpha() or current_character == "_":
                end_index = source_index + 1
                while end_index < len(self.source) and (
                    self.source[end_index].isalnum() or self.source[end_index] == "_"
                ):
                    end_index += 1
                token_text = self.source[source_index:end_index]
                token_kind = token_text if token_text in KEYWORDS else "IDENT"
            elif current_character.isdigit():
                end_index = source_index
                while end_index < len(self.source) and self.source[end_index].isdigit():
                    end_index += 1
                token_kind = "INT"
                if end_index < len(self.source) and self.source[end_index] == ".":
                    token_kind = "REAL"
                    end_index += 1
                    if end_index == len(self.source) or not self.source[end_index].isdigit():
                        self.errors.append(Diagnostic(
                            "literal real inválido",
                            Token("INVALID", ".", *token_start),
                            "lexical",
                        ))
                        update_position(self.source[source_index:end_index])
                        source_index = end_index
                        continue
                    while end_index < len(self.source) and self.source[end_index].isdigit():
                        end_index += 1
                token_text = self.source[source_index:end_index]
            else:
                operators = ("<=", ">=", "==", "!=", "&&", "||")
                token_text = next(
                    (operator for operator in operators
                     if self.source.startswith(operator, source_index)),
                    current_character,
                )
                valid_single = set("+-*/=<>!;,:(){}[]")
                if token_text not in valid_single and token_text not in operators:
                    self.errors.append(Diagnostic(
                        f"caractere inválido {current_character!r}",
                        Token("INVALID", current_character, *token_start),
                        "lexical",
                    ))
                    update_position(current_character)
                    source_index += 1
                    continue
                token_kind = token_text
                end_index = source_index + len(token_text)

            tokens.append(Token(token_kind, token_text, *token_start))
            update_position(self.source[source_index:end_index])
            source_index = end_index

        tokens.append(Token("EOF", "", line_number, column_number))
        return tokens


def tokenize(source):
    """Compatibilidade com a API antiga; use ``Scanner(source).scan()``."""
    scanner = Scanner(source)
    tokens = scanner.scan()
    if scanner.errors:
        raise ValueError(str(scanner.errors[0]))
    return tokens
