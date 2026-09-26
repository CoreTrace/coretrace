# SPDX-License-Identifier: Apache-2.0
# Standard input reaches the code eval runs: code-injection, a rule of the analyzer's 0.11.0.
def calculate():
    return eval(input())
