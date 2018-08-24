#include <iostream>
namespace boost
{
    void __cdecl throw_exception(class std::exception const & e){
        std::cout<<"Caught:"<<e.what()<<std::endl;

    }
}
