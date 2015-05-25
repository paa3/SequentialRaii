# Sequential Initialization in C++
## Introduction
Sequential resource allocation and initialization is commonly encountered when working with C provided APIs. In general multiple steps must be taken when aquiring and preparing a resource for use. A common usecase could be to create e.g. an UDP socket for communication:
- First the socket must be created.
- Then we may need to configure it using ioctl or other API calls.
- Perhaps we also need to do a server name look-up too.
- We may need to allocate memory for a persistent buffer.
- ...

All of these steps in preparing the resource must be done in a more or less strict order. They can also fail at any point and we must be ready to perform a full cleanup of the preceeding steps in the inverse order to avoid locking up any resources. Ultimately we want to check for success or failure at each step and upon failure we want to undo all the previous steps in the reverse order. When we're done using the resource, clean-up should be run as well.

Common implementations will often attempt to encapsulate the final resource inside a re-usable class in a good RAII-like manner. Take a moment and try come up with a solution on how you would solve this seemingly simple task.

There are many aspects to resource handling. The questions below are pointers to some possible issues.
- Does your solution define ```initialize()``` and ```uninitialize()``` functions?
- Does the uninitialization code, or some part of it, appear more than once?
- If the initialization sequence was to become more complex, are you sure the uninitalization code will be run in exact inverse order?
- Did you remember to clean up all aquired resources?
- What if an exception is thrown in there somewhere?
- What about that new guy who will maintain your code in a year from now and need to add a new initialization step?

Enter SequentialRaii, the result of attempting to answer the questions above in a general manner. It is basically just a container for storing lambda expressions and executing those expressions in the correct order when calling ```initialize()``` and ```uninitialize()```.

## Advantages
Through extensive use of lambda expressions it offers the following advantages:
- All the code for initializing and cleaning up a resource is localized in space and time. The code is also specified only once. This results in increased maintainability and significantly reduced possibility for bugs.
- Offers automatic cleanup in correct inverse order.

## Disadvantages
Lambda expressions aren't without a disadvantage:
 - In general they are plain ugly too look at.
 - As with any lambda use special care must be taken to manage the lifetime of any variables captured by reference. For example, the following code will generate a runtime exception since c's destructor is run before seqraii's when leaving the scope:
```c++
  {
    SequentialRaii seqraii;
    std::vector<int> c;
    seqraii.addStep([&](){c.push_back(1); return true;}, [&](){c.pop_back();});
    seqraii.initialize();
  }
```
The modified code below will run just fine within the limits defined by C++:
```c++
  {
    std::vector<int> c;    // Swapped this line with the one below
    SequentialRaii seqraii;
    seqraii.addStep([&](){c.push_back(1); return true;}, [&](){c.pop_back();});
    seqraii.initialize();
  }
```

## Example
Please see the (somewhat artifical) example provided under ```\example\```.
