import dicey

if __name__ == '__main__':
    dc = dicey.connect("@/tmp/.uvsock")
    srv = dc.object('/dicey/server')
    print(srv.list_plugins())

