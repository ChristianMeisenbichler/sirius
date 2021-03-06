#ifndef _MDARRAY_BASE_H_
#define _MDARRAY_BASE_H_

#include <assert.h>

// TODO: size_t for all offset- and size-related variables
class dimension 
{
    public:
  
        dimension() : start_(0), end_(-1), size_(0) 
        {
        }
        
        dimension(unsigned int size_) : size_(size_)
        {
            start_ = 0;
            end_ = size_ - 1;
        }
    
        dimension(int start_, int end_) : start_(start_), end_(end_) 
        {
            assert(end_ >= start_);
            size_ = end_ - start_ + 1;
        };

        inline int start() 
        {
            return start_;
        }
        
        inline int end() 
        {
            return end_;
        }
        
        inline unsigned int size() 
        {
            return size_;
        }
        
    private:

        int start_;
        int end_;
        unsigned int size_;
};

template <typename T, int ND> class mdarray_base
{
    public:
    
        mdarray_base() : mdarray_ptr(NULL), 
                         allocated_(false), 
                         mdarray_ptr_device(NULL), 
                         allocated_on_device(false) 
        { 
        }
        
        ~mdarray_base()
        {
            deallocate();
        }
        
        void init_dimensions(const std::vector<dimension>& vd) 
        {
            assert(vd.size() == ND);
            
            for (int i = 0; i < ND; i++) d[i] = vd[i];
            
            offset[0] = -d[0].start();
            unsigned int n = 1;
            for (int i = 1; i < ND; i++) 
            {
                n *= d[i-1].size();
                offset[i] = n;
                offset[0] -= offset[i] * d[i].start();
            }
        }
 
        inline size_t size()
        {
            size_t size_ = 1;

            for (int i = 0; i < ND; i++) 
                size_ *= d[i].size();

            return size_;
        }

        inline int size(int i)
        {
           assert(i < ND);
           return d[i].size();
        }

        inline std::vector<int> dimensions()
        {
            std::vector<int> vd(ND);
            for (int i = 0; i < ND; i++)
                vd[i] = d[i].size();
            return vd;
        }

        void allocate()
        {
            deallocate();
            
            size_t sz = size();
             
            if (sz && (!mdarray_ptr)) 
            {
                mdarray_ptr = new T[sz];
                allocated_ = true;
            }
        }

        void deallocate()
        {
            if (allocated_)
            {
                delete[] mdarray_ptr;
                mdarray_ptr = NULL;
                allocated_ = false;
            }
        }
        
        void zero()
        {
            assert(mdarray_ptr);
            memset(mdarray_ptr, 0, size() * sizeof(T));
        }
        
        void set_ptr(T* ptr)
        {
            mdarray_ptr = ptr;
        }
        
        T* get_ptr()
        {
            return mdarray_ptr;
        }
        
        bool allocated()
        {
            return allocated_;
        }

        unsigned int hash()
        {
            unsigned int h = 5381;

            for(size_t i = 0; i < size() * sizeof(T); i++)
                h = ((h << 5) + h) + ((unsigned char*)mdarray_ptr)[i];

            return h;
        }

        
        /*void copy_members(const mdarray_base<impl,T,ND>& src) 
        {
            for (int i = 0; i < ND; i++) 
            { 
                offset[i] = src.offset[i];
                d[i] = src.d[i];
            }
        }*/
 
    protected:
    
        T* mdarray_ptr;
        
        bool allocated_;
        
        T* mdarray_ptr_device;  
        
        bool allocated_on_device;
        
        dimension d[ND];
        
        int offset[ND];

    private:

        // forbid copy constructor
        mdarray_base(const mdarray_base<T,ND>& src);
        
        // forbid assign operator
        mdarray_base<T,ND>& operator=(const mdarray_base<T,ND>& src); 
        
};

#endif // _MDARRAY_BASE_H_


