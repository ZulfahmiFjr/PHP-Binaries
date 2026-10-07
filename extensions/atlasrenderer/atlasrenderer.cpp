extern "C" {
#include "php_atlasrenderer.h"
#include "ext/standard/info.h"
}
#include "renderer.h"
#include <chrono>
#include <exception>
#include <cmath>
#include <stdexcept>

static atlas::Camera camera_from_array(HashTable *ht) {
    atlas::Camera c;
    auto value=[ht](const char*key,double fallback){zval*v=zend_hash_str_find(ht,key,strlen(key));if(v&&Z_TYPE_P(v)!=IS_LONG&&Z_TYPE_P(v)!=IS_DOUBLE)throw std::invalid_argument("Camera options must be numbers");return v?zval_get_double(v):fallback;};
    auto integer=[&value](const char *key,int fallback){double n=value(key,fallback);if(!std::isfinite(n)||n<1||n>8192||std::floor(n)!=n)throw std::invalid_argument("Invalid integer camera option");return int(n);};
    c.width=integer("width",640);c.height=integer("height",384);c.x=value("x",0);c.y=value("y",64);c.z=value("z",0);
    c.scale=value("scale",4);c.azimuth=value("azimuth",135);c.inclination=value("inclination",60);c.max_steps=integer("max_steps",2048);return c;
}
PHP_FUNCTION(atlas_render) {
    zend_string *snapshot;HashTable *camera,*base=nullptr;
    ZEND_PARSE_PARAMETERS_START(2,3) Z_PARAM_STR(snapshot) Z_PARAM_ARRAY_HT(camera) Z_PARAM_OPTIONAL Z_PARAM_ARRAY_HT_OR_NULL(base) ZEND_PARSE_PARAMETERS_END();
    try {
        auto started=std::chrono::steady_clock::now(); atlas::Result previous; std::array<int,4> region; bool clipped=false;
        if(base) {
            auto buffer=[base](const char*key){zval*v=zend_hash_str_find(base,key,strlen(key));if(!v||Z_TYPE_P(v)!=IS_STRING)throw std::invalid_argument("Missing previous buffer");return std::string(Z_STRVAL_P(v),Z_STRLEN_P(v));};
            previous.rgb=buffer("rgb");previous.hits=buffer("hits");previous.preview=buffer("preview");
        }
        zval *clip=zend_hash_str_find(camera,"region",6);
        if(clip) {
            if(Z_TYPE_P(clip)!=IS_ARRAY||zend_hash_num_elements(Z_ARRVAL_P(clip))!=4)throw std::invalid_argument("Region needs four integers");
            for(int i=0;i<4;++i){zval*v=zend_hash_index_find(Z_ARRVAL_P(clip),i);if(!v||Z_TYPE_P(v)!=IS_LONG||Z_LVAL_P(v)<0||Z_LVAL_P(v)>1024)throw std::invalid_argument("Invalid region integer");region[i]=int(Z_LVAL_P(v));}clipped=true;
        }
        auto result=atlas::render(std::string(ZSTR_VAL(snapshot),ZSTR_LEN(snapshot)),camera_from_array(camera),base?&previous:nullptr,clipped?&region:nullptr);
        array_init(return_value);
        add_assoc_stringl(return_value,"rgb",result.rgb.data(),result.rgb.size());add_assoc_stringl(return_value,"hits",result.hits.data(),result.hits.size());
        add_assoc_stringl(return_value,"preview",result.preview.data(),result.preview.size());add_assoc_long(return_value,"traced_voxels",result.traced_voxels);
        add_assoc_double(return_value,"render_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
    }catch(const std::exception&e){zend_value_error("AtlasRenderer: %s",e.what());RETURN_THROWS();}
}
PHP_FUNCTION(atlas_split_tiles) {
    zend_string *rgb;zend_long width,height;
    ZEND_PARSE_PARAMETERS_START(3,3) Z_PARAM_STR(rgb) Z_PARAM_LONG(width) Z_PARAM_LONG(height) ZEND_PARSE_PARAMETERS_END();
    try {if(width<128||width>1024||height<128||height>1024)throw std::invalid_argument("Invalid dimensions");auto tiles=atlas::split_tiles(std::string(ZSTR_VAL(rgb),ZSTR_LEN(rgb)),int(width),int(height));array_init(return_value);for(const auto&t:tiles)add_next_index_stringl(return_value,t.data(),t.size());}
    catch(const std::exception&e){zend_value_error("AtlasRenderer: %s",e.what());RETURN_THROWS();}
}
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_atlas_render,0,2,IS_ARRAY,0)
    ZEND_ARG_TYPE_INFO(0,snapshot,IS_STRING,0)
    ZEND_ARG_TYPE_INFO(0,camera,IS_ARRAY,0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0,previous,IS_ARRAY,1,"null")
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_atlas_tiles,0,3,IS_ARRAY,0)
    ZEND_ARG_TYPE_INFO(0,rgb,IS_STRING,0)
    ZEND_ARG_TYPE_INFO(0,width,IS_LONG,0)
    ZEND_ARG_TYPE_INFO(0,height,IS_LONG,0)
ZEND_END_ARG_INFO()
static const zend_function_entry functions[]={PHP_FE(atlas_render,arginfo_atlas_render) PHP_FE(atlas_split_tiles,arginfo_atlas_tiles) PHP_FE_END};
PHP_MINIT_FUNCTION(atlasrenderer){REGISTER_LONG_CONSTANT("ATLAS_RENDERER_API_VERSION",atlas::API_VERSION,CONST_CS|CONST_PERSISTENT);return SUCCESS;}
PHP_MINFO_FUNCTION(atlasrenderer){php_info_print_table_start();php_info_print_table_row(2,"AtlasBoard renderer","enabled");php_info_print_table_row(2,"API version","2");php_info_print_table_end();}
extern "C" {
zend_module_entry atlasrenderer_module_entry={STANDARD_MODULE_HEADER,"atlasrenderer",functions,PHP_MINIT(atlasrenderer),nullptr,nullptr,nullptr,PHP_MINFO(atlasrenderer),"1.1.0",STANDARD_MODULE_PROPERTIES};
#ifdef COMPILE_DL_ATLASRENDERER
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(atlasrenderer)
#endif
}
