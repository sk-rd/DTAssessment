import dta


def test_add():
    assert dta.add(2, 3) == 5
    assert dta.add(-5, 3) == -2
