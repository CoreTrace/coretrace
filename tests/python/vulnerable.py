# SPDX-License-Identifier: Apache-2.0
# Fixture for coretrace-python-analyzer: one reported finding, one suppressed in source.


def run(user_input):
    return eval(user_input)


def quiet(expr):
    return eval(expr)  # coretrace: ignore[dangerous-eval]
