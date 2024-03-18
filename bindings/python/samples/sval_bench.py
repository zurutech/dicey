import dicey

class Cit:
    elem: list

    def __init__(self, n = 8):
        self.elem = [0] * n

    def next(self) -> str:
        cur = str(self)

        for i in range(len(self.elem)):
            nxt = self.elem[i] + 1

            if nxt < 26:
                self.elem[i] = nxt
                break

            self.elem[i] = 0

        return cur

    def __str__(self) -> str:
        return ''.join(map(lambda c: chr(ord('A') + c), reversed(self.elem)))

if __name__ == '__main__':
    cit = Cit()

    with dicey.connect('@/tmp/.uvsock') as dc:
        for i in range(80000):
            dc.set('/sval', ('sval.Sval', 'Value'), cit.next())
