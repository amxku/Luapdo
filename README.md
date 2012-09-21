Luapdo
===============================

Luapdo v0.1

PDO for Lua

* sqlite
* mysql
* mssql

###Usage###

    local pdo = require "pdo"
    function test_query(dbtype, connstr)
    	local db, errno, errmsg = pdo.open(dbtype, connstr)
    	if db == nil then
    		print(errno, errmsg)
    		return
    	end
    	
    	print(pdo.escape(db, "good 'ok' for escape"))
    	
    	local errno, rows = pdo.query(db, "select 11,22,33", function (names, values)
    		print(values[1], values[2], values[3])
    		-- return true to break query here
    		end)
    		
    	print(errno, pdo.error(db, errno))
    	print(pdo.errno(db))
    	print(pdo.check_conn(db))

    	pdo.close(db)
    end
    
    -- test sqlite3
    test_query("sqlite3", ":memory:")

    -- test mysql
    test_query("mysql", "host=127.0.0.1;port=3306;user=root;pass=root")

    -- test ado
    test_query("ado", "Provider=Microsoft.Jet.OLEDB.4.0;Data Source=test.mdb")


http://os.sebug.net/

http://ssv.sebug.net/Luapdo