"""Four-way result vocabulary. Plan section 9."""
PASS = 'PASS'
FAIL = 'FAIL'
NOT_IMPLEMENTED = 'NOT_IMPLEMENTED'
NOT_DEMONSTRATED = 'NOT_DEMONSTRATED'

ORDER = [PASS, FAIL, NOT_IMPLEMENTED, NOT_DEMONSTRATED]


class Result:
    def __init__(self, test_id, title, status, detail='', data=None,
                 gate='', spec_ref=''):
        self.test_id = test_id
        self.title = title
        self.status = status
        self.detail = detail
        self.data = data or {}
        self.gate = gate
        self.spec_ref = spec_ref

    def as_dict(self):
        return dict(test_id=self.test_id, title=self.title, status=self.status,
                    detail=self.detail, gate=self.gate, spec_ref=self.spec_ref,
                    data=self.data)

    def __repr__(self):
        return '<%s %s>' % (self.test_id, self.status)
