
## SQLite

All SQLite wrappers reside in namespace `rad::sqlite`

```
using namespace rad::sqlite;
```

To construct a database with file:

```
// flags: read, write, read_write_create
// existing databases files are not overwritten
// default flags is read_write_create
database db{ "/path/to/db", flags::read_write_create };
// or:
database db;
db.open("/path/to/db", flags::read_write_create);
```
To construct an in memory database:

```
// use tag in_memory_database
database db{ in_memory_database };
// use ":memory:" path
database db{ ":memory:" };
// or:
database db;
db.open(in_memory_database);
```
To get access to the native sqlite handle:
```
db.native_handle().get();
```

To make queries:
```
// without binding parameters
auto q = db.make_query("SELECT * FROM table WHERE ID = 1");
// with binding parameters
std::string name = "username";
auto q = db.make_query("SELECT * FROM table WHERE ID = ? AND NAME = ?", 1, name);
// the sqlite library will make a copy of the bound
// string name parameter so name is not required to be
// valid after binding
// to bind by view instead of making copy use as_view
auto q = db.make_query("SELECT * FROM table WHERE ID = ? AND NAME = ?", 1, as_view(name));
// name buffer will now be referenced by sqlite library
// and must be valid as long as it is bound to the query
```
For more advanced use the query provides many methods to bind and retrieve values:

```
query q{ db };
q.prepare("SELECT age, height FROM table WHERE ID = ? AND USER = ?");
// bind index is 0 based
q.bind(0, 1);
std::string name = "username";
q.bind(1, name);
// clear bind parameter at 1 and replace with new parameter
q.bind(1, as_view(name));
// binding using << operator starts at index 0
q << 2; // index is now 1
q << name; // index is now 2, can't bind new values
// or in one line:
q << 2 << name;
// to reset the bind index use index():
q << index(1) << as_view(name);
// bind index is now 2 again
To clear bindings:
q.clear_bindings();
```

To execute the query:
```
// execute the query and discard the results
// for SELECT queries the query will step one time
q.execute();
// execute the query and return the count of changes
std::size_t n = q.delete_rows();
// execute the query and return the count of changes
std::size_t n = q.insert();
// execute the query and return the count of changes
std::size_t n = q.update();
// delete_rows(), insert(), update() are the same.
```

To execute a SELECT query and return results:
```
while (q.next()) {
    std::size_t columns_n = q.columns_count();
    // get index is 0 based
    int age = q.get_value<int>(0);
    int height = q.get_value<int>(1);
    // or use operator >> which starts at index 0
    q >> age >> height;
    // reset the index:
    q >> index(0) >> age >> height;
    q >> index(1) >> height;
}
```

For a range SELECT loop:
```
// r holds a reference to q
for (row r : q.select()) {
    int age = 0;
    int height = 0;
    // use operator >> which starts at index 0
    r >> age >> height;
    // reset the index:
    r >> index(0) >> age >> height;
    r >> index(1) >> height;
}
```

Supported types for bind and get are: integral types, floating point types, `std::nullptr_t`, `std::span<(const)? uint8_t>`, `std::vector<uint8_t>` and standard string types.

To bind by view wrap the parameter in `as_view`.

To bind an optional value use `std::optional` of supported type and if it has a value this value will be bound, otherwise null will be bound.

Returned view values values like `std::span` and `std::strig_view` points to an internal sqlite buffer that may be invalidated by any operation (like step) on the query.

To support more user defined types `SQLiteBinder` and `SQLiteColumnExtractor` can be specialized for new types.

More functionality are provided by the wrappers like count of bound parameters, getting name of bound parameter by index, last insert id, count of changes, count of results columns, checking columns types and more.

If the query statement string contains multiple SQL statements, only the first statement will be executed.

To execute multiple SQL statements and discard the results:
```
const char* multiple_statements = ...
// returns the count of executed quries
std::size_t quries_n = db.execute(multiple_statements);
```

To define your SQL function:

```
db.define_fn("MYFN", [](int a, std::string b) {
    return std::to_string(a) + "-" + b;
});
int number = 32;
std::string text = "Thirty-Two";
auto q = db.make_query("INSERT MYFN(?, ?) INTO TABLE");
q << number << text;
q.execute();
```
or:
```
db.define_fn("POWFN", [](double b, double p) {
    return std::pow(b, p);
});
auto q = db.make_query("SELECT POWFN(5, 2)");
q.execute();
double result = q.get_value<double>(0);
```

The function paramters are converted from SQLite values to `C++` values using `SQLiteValueExtractor` sepcialization.

The function return value is converted from `C++` value to SQLite value using `SQLiteResultStore` sepcialization.

Supported types are similar to bound parameters and new types can be supported by making sepcializations.

To use RAII like transactions:
```
{
    transaction tr{ db };
    
    // make queries on db
    // if any exception is thrown
    // the transaction will be rolled back

    // without explicit commit 
    // the transaction will be rolled back
    tr.commit();

    // tr is destructed
    // rollback if not commited
}
```