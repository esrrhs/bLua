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

    benchmark()
end

function benchmark()
    local begin = os.time()
    local a = newA()
    for i = 1, 100000000 do
        a:get_int()
        a:set_int(i)
    end
    print(os.time() - begin)
end

local ok, err = pcall(main)
if not ok then
    print(err)
end
