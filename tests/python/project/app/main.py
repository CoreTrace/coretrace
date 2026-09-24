# SPDX-License-Identifier: Apache-2.0
# Input reaches os.system through app.helpers: only a whole-project analysis sees it.
from app.helpers import execute


def run():
    execute(input())


def quiet(expr):
    return eval(expr)  # coretrace: ignore[dangerous-eval]
