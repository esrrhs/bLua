function main()
    print("start")

    local a = newA()

    a:set_int(123)
    print("a:get_int() ", a:get_int())

    a:set_string("abc")
    print("a:set_string() ", a:get_string())

    printA(a)

    local aa = a:get_this()
    print("aa == a ", aa == a)
    printA(aa)

    print("done")
end

function benchmark(n, log)
    local begin = os.time()
    local a = newA()
    local sum = 0
    for i = 1, n do
        a:get_int()
        a:set_int(i)
        sum = sum + i
    end
    local cost = os.time() - begin
    print(cost, log, sum)
    printA(a)
    return cost, log, sum, a
end

local ok, err = pcall(main)
if not ok then
    print(err)
end
