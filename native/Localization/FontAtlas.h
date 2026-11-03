// Runtime-only font generation. Game sprite wrappers and fallback symbols remain local.
// Latin, kana and CJK glyphs use the selected OFL Source Han Sans face.
#include <map>
#include <set>
#include <cmath>
#include <CommonCrypto/CommonDigest.h>

static NSString *sha256(NSData *data) {
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, digest);
    NSMutableString *s=[NSMutableString new];
    for(unsigned char c:digest)[s appendFormat:@"%02x",c];
    return s;
}

static CTFontRef loadFace(NSString *path) {
    CGDataProviderRef provider=CGDataProviderCreateWithFilename(path.fileSystemRepresentation);
    CGFontRef cg=provider?CGFontCreateWithDataProvider(provider):nullptr;
    if(provider)CGDataProviderRelease(provider);
    if(!cg)throw std::runtime_error("Cannot load selected OTF/TTF font");
    CTFontRef font=CTFontCreateWithGraphicsFont(cg,22,nullptr,nullptr);
    CGFontRelease(cg);
    return font;
}

static size_t textureOffset(const Bytes &b) {
    for(size_t i=0;i+24<b.size();i+=4)
        if(u32(b,i)==0x02505854 && u32(b,i+4)==4096 && u32(b,i+8)==2048 && u32(b,i+12)==9) {
            if(i+24+8388608>b.size())break;
            return i;
        }
    throw std::runtime_error("Unsupported BC3 font texture");
}

static Bytes decodeAlpha(const Bytes &b,size_t data) {
    const int w=4096,h=2048;
    Bytes rgba(w*h*4,255);
    for(int y=0;y<h;y+=4)for(int x=0;x<w;x+=4){
        size_t o=data+(y/4*(w/4)+x/4)*16;
        int a[8]={b[o],b[o+1],0,0,0,0,0,0};
        if(a[0]>a[1])for(int k=1;k<=6;k++)a[k+1]=((7-k)*a[0]+k*a[1])/7;
        else {for(int k=1;k<=4;k++)a[k+1]=((5-k)*a[0]+k*a[1])/5;a[6]=0;a[7]=255;}
        uint64_t bits=0;for(int k=0;k<6;k++)bits|=(uint64_t)b[o+2+k]<<(8*k);
        for(int py=0;py<4;py++)for(int px=0;px<4;px++)
            rgba[((y+py)*w+x+px)*4+3]=a[(bits>>(3*(py*4+px)))&7];
    }
    return rgba;
}

static void encodeAlpha(Bytes &b,size_t data,const Bytes &rgba) {
    const int w=4096,h=2048,a[8]={255,0,218,182,145,109,72,36};
    for(int y=0;y<h;y+=4)for(int x=0;x<w;x+=4){
        uint64_t bits=0;
        for(int py=0;py<4;py++)for(int px=0;px<4;px++){
            int alpha=rgba[((y+py)*w+x+px)*4+3],best=0;
            for(int k=1;k<8;k++)if(abs(alpha-a[k])<abs(alpha-a[best]))best=k;
            bits|=(uint64_t)best<<(3*(py*4+px));
        }
        size_t o=data+(y/4*(w/4)+x/4)*16;
        b[o]=255;b[o+1]=0;
        for(int k=0;k<6;k++)b[o+2+k]=(bits>>(8*k))&255;
        b[o+8]=b[o+9]=255;b[o+10]=b[o+11]=0;
        memset(b.data()+o+12,0,4);
    }
}

struct GlyphCell {uint16_t code;uint8_t half,col,row,offset,width;};

struct FontMetrics {double scale,baseline;};
static FontMetrics sharedMetrics(CTFontRef regular,CTFontRef bold,const std::set<uint16_t>&required){
    double low=0,high=0;
    std::set<uint16_t> sample=required;
    for(uint16_t c=32;c<127;c++)sample.insert(c);
    for(CTFontRef font:{regular,bold})for(uint16_t c:sample){
        CGGlyph g=0;if(!CTFontGetGlyphsForCharacters(font,&c,&g,1)||!g)continue;
        CGRect b=CTFontGetBoundingRectsForGlyphs(font,kCTFontOrientationHorizontal,&g,nullptr,1);
        if(CGRectIsEmpty(b))continue;
        low=std::min(low,(double)CGRectGetMinY(b));high=std::max(high,(double)CGRectGetMaxY(b));
    }
    double scale=std::min(1.0,23.0/std::max(1.0,high-low));
    return {scale,.5-low*scale};
}

static void renderAtlas(Bytes &atlas,const std::vector<GlyphCell>&cells,CTFontRef font,
                        const std::set<uint16_t>&required,FontMetrics metrics) {
    size_t tex=textureOffset(atlas);
    Bytes rgba=decodeAlpha(atlas,tex+24),tile(26*26*4);
    CGColorSpaceRef cs=CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx=CGBitmapContextCreate(tile.data(),26,26,8,26*4,cs,
        kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if(!ctx)throw std::runtime_error("Font raster context failed");
    CGContextSetRGBFillColor(ctx,1,1,1,1);
    unsigned replaced=0,retained=0;
    for(const GlyphCell &cell:cells){
        UniChar c=cell.code;CGGlyph glyph=0;
        if(!CTFontGetGlyphsForCharacters(font,&c,&glyph,1)||!glyph){
            if(required.count(c)) {CGContextRelease(ctx);throw std::runtime_error("Selected font lacks required translated character");}
            retained++;continue;
        }
        std::fill(tile.begin(),tile.end(),0);
        CGRect box=CTFontGetBoundingRectsForGlyphs(font,kCTFontOrientationHorizontal,&glyph,nullptr,1);
        double available=cell.width;
        // Preserve one vertical scale and baseline for every character. Narrow slots
        // may adjust horizontal scale only; M/W must never become shorter than I.
        double scaleY=metrics.scale;
        double scaleX=scaleY*std::min(1.0,std::max(1.0,available-1)/std::max(1.0,box.size.width*scaleY));
        CGContextSaveGState(ctx);
        CGContextScaleCTM(ctx,scaleX,scaleY);
        CGPoint p=CGPointMake(((available-box.size.width*scaleX)/2)/scaleX-box.origin.x,
                              metrics.baseline/scaleY);
        CTFontDrawGlyphs(font,&glyph,&p,1,ctx);
        CGContextRestoreGState(ctx);
        // TXP pixels are stored bottom-up, while sprite/font coordinates are top-down.
        // Reflect the entire texture Y coordinate, including each glyph row.
        int left=cell.col*26,top=cell.row*26;
        if(left+26>4096||top+26>2048){CGContextRelease(ctx);throw std::runtime_error("Glyph atlas capacity exceeded");}
        for(int y=0;y<26;y++)for(int x=0;x<26;x++){
            size_t out=((2047-top-y)*4096+left+x)*4;
            rgba[out]=rgba[out+1]=rgba[out+2]=255;
            rgba[out+3]=tile[(y*26+x)*4+3];
        }
        replaced++;
    }
    CGContextRelease(ctx);
    encodeAlpha(atlas,tex+24,rgba);
    printf("Source Han font atlas: %u rendered, %u original symbols retained\n",replaced,retained);
}

static void makeFonts(NSString *source,NSString *dest,NSString *glyphs,NSString *assets,
                      NSString *customFont,NSString *requiredText) {
    Bytes map=unpack(join(source,@"fontmap.farc"));
    if(u32(map,0)!=0x33484d46)throw std::runtime_error("Unsupported font map");
    uint32_t fontOffset=0;
    for(unsigned i=0;i<u32(map,8);i++){
        uint32_t o=u32(map,u32(map,12)+i*4);
        if(u32(map,o)==0)fontOffset=o;
    }
    if(!fontOffset)throw std::runtime_error("Missing CJK font map");
    uint32_t n=u32(map,fontOffset+20),old=u32(map,fontOffset+24);
    if(n!=7145||old+n*8>map.size())throw std::runtime_error("Unrecognized source font map");
    std::map<uint16_t,GlyphCell> cells;
    for(unsigned i=0;i<n;i++){
        size_t o=old+i*8;uint16_t c=map[o]|map[o+1]<<8;
        cells[c]={c,map[o+2],map[o+4],map[o+5],map[o+6],map[o+7]};
    }
    // Use every required translated code point, including characters omitted by old manifests.
    std::set<uint16_t> required; size_t nextCell=46*157;
    NSString *all=[glyphs stringByAppendingString:requiredText];
    for(NSUInteger i=0;i<all.length;i++){
        uint16_t c=[all characterAtIndex:i];if(c<32||c=='\\')continue;
        required.insert(c);
        if(!cells.count(c)){
            size_t next=nextCell++;
            cells[c]={c,(uint8_t)(c<128),uint8_t(next%157),uint8_t(next/157),0,uint8_t(c<128?12:24)};
        }
    }
    std::vector<GlyphCell> table;
    for(auto &pair:cells)table.push_back(pair.second);
    CTFontRef regular=loadFace(customFont?:join(assets,@"fonts/SourceHanSansSC-Regular.otf"));
    CTFontRef bold=loadFace(customFont?:join(assets,@"fonts/SourceHanSansSC-Bold.otf"));
    FontMetrics metrics=sharedMetrics(regular,bold,required);
    printf("Font shared metrics: scale=%.6f baseline=%.6f\n",metrics.scale,metrics.baseline);
    Bytes records;
    for(GlyphCell &c:table){
        // Retain the engine's compact half-width ASCII layout. Vertical font metrics
        // remain shared; wide Latin glyphs adjust horizontal shape only.
        c.offset=0;c.width=c.half?12:24;
        if(c.code>=32&&c.code<127){c.half=1;c.width=12;}
        records.insert(records.end(),{uint8_t(c.code),uint8_t(c.code>>8),c.half,0,c.col,c.row,c.offset,c.width});
    }
    put(map,fontOffset+20,(uint32_t)table.size());put(map,fontOffset+24,(uint32_t)map.size());
    map.insert(map.end(),records.begin(),records.end());
    write(join(dest,@"fontmap.farc"),pack("fontmap.bin",map));
    for(NSString *name in @[@"spr_fnt_24",@"spr_fnt_bold24"]){
        CTFontRef font=[name containsString:@"bold"]?bold:regular;
        Bytes atlas=unpack(join(source,[NSString stringWithFormat:@"2d/%@.farc",name]));
        renderAtlas(atlas,table,font,required,metrics);
        write(join(dest,[NSString stringWithFormat:@"2d/%@.farc",name]),pack([[name stringByAppendingString:@".bin"]UTF8String],atlas));
    }
    CFRelease(regular);CFRelease(bold);
}
