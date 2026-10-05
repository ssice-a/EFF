"""Small, side-effect-free visibility expressions and shared state key updates."""
from __future__ import annotations

import math
import re

from .type_tree import require

VARIABLE = re.compile(r"\$[A-Za-z_][A-Za-z0-9_]*\Z")
TOKEN = re.compile(r"\s*(\$[A-Za-z_][A-Za-z0-9_]*|true\b|false\b|-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|==|!=|<=|>=|&&|\|\||[()!<>])")


def number(text):
    value = float(text)
    require(math.isfinite(value), "state value must be finite")
    return value


class Condition:
    def __init__(self, text):
        self.text, self.tokens, self.position, self.variables = text, [], 0, set()
        offset = 0
        while offset < len(text.rstrip()):
            match = TOKEN.match(text, offset)
            require(match is not None, "invalid visibility expression near: " + text[offset:])
            self.tokens.append(match[1])
            offset = match.end()
        self.tree = self.parse_or()
        require(self.position == len(self.tokens), "unexpected visibility expression token")

    def take(self, *choices):
        if self.position < len(self.tokens) and self.tokens[self.position] in choices:
            value = self.tokens[self.position]
            self.position += 1
            return value
        return None

    def parse_or(self):
        tree = self.parse_and()
        while self.take("||"):
            tree = ("||", tree, self.parse_and())
        return tree

    def parse_and(self):
        tree = self.parse_compare()
        while self.take("&&"):
            tree = ("&&", tree, self.parse_compare())
        return tree

    def parse_compare(self):
        left = self.parse_primary()
        operator = self.take("==", "!=", "<", "<=", ">", ">=")
        return (operator, left, self.parse_primary()) if operator else left

    def parse_primary(self):
        if self.take("!"):
            return ("!", self.parse_primary())
        if self.take("("):
            tree = self.parse_or()
            require(self.take(")"), "visibility expression has unmatched parentheses")
            return tree
        require(self.position < len(self.tokens), "visibility expression is incomplete")
        token = self.tokens[self.position]
        self.position += 1
        if VARIABLE.fullmatch(token):
            self.variables.add(token)
            return ("var", token)
        if token in ("true", "false"):
            return ("literal", token == "true")
        try:
            return ("literal", number(token))
        except ValueError as error:
            raise ValueError("invalid visibility expression token: " + token) from error

    def evaluate(self, states):
        def visit(node):
            operator = node[0]
            if operator == "literal": return node[1]
            if operator == "var":
                require(node[1] in states, "undefined state: " + node[1])
                return states[node[1]]
            if operator == "!": return not visit(node[1])
            if operator == "&&": return bool(visit(node[1])) and bool(visit(node[2]))
            if operator == "||": return bool(visit(node[1])) or bool(visit(node[2]))
            a, b = visit(node[1]), visit(node[2])
            return {"==": lambda: a == b, "!=": lambda: a != b, "<": lambda: a < b,
                    "<=": lambda: a <= b, ">": lambda: a > b, ">=": lambda: a >= b}[operator]()
        return bool(visit(self.tree))


def apply_key(states, binding, seconds=0):
    """One input updates one existing state; configuration order resolves ties."""
    variable, values = binding["variable"], binding["values"]
    require(variable in states, "key writes undefined state")
    if binding["type"] == "cycle":
        require(states[variable] in values, "cycle state is outside its declared states")
        states[variable] = values[(values.index(states[variable]) + 1) % len(values)]
    elif binding["type"] == "hold":
        target = values[0]
        step = binding["speed"] * seconds
        require(seconds >= 0, "negative input duration")
        states[variable] = min(target, states[variable] + step) if target > states[variable] else max(target, states[variable] - step)
    else:
        states[variable] = values[0]
