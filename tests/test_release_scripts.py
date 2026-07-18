from scripts.benchmark_gate import parse_summary, regressions


def test_benchmark_summary_parser_extracts_p999_column():
    output = """
B1:FOC_30kHz                  0.06  0.10  0.14  0.04  0 PASS
B7:encode_pipe                0.08  0.10  0.24  0.14  0 PASS
"""
    assert parse_summary(output) == {
        "B1:FOC_30kHz": 0.14,
        "B7:encode_pipe": 0.24,
    }


def test_benchmark_gate_rejects_more_than_twenty_percent_regression():
    baseline = {"B1:FOC_30kHz": 1.0, "B7:encode_pipe": 2.0}
    assert regressions(baseline, {"B1:FOC_30kHz": 1.2, "B7:encode_pipe": 2.4}, 20.0) == []
    failures = regressions(
        baseline, {"B1:FOC_30kHz": 1.21, "B7:encode_pipe": 2.0}, 20.0
    )
    assert len(failures) == 1
    assert failures[0].startswith("B1:FOC_30kHz:")
