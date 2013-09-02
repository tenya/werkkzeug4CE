/*+**************************************************************************/
/***                                                                      ***/
/***   This file is distributed under a BSD license.                      ***/
/***   See LICENSE.txt for details.                                       ***/
/***                                                                      ***/
/**************************************************************************+*/

#include "wz4_mesh.hpp"
#include "wz4_mtrl2_ops.hpp"
#include "util/scanner.hpp"
#include "wz4lib/basic_ops.hpp"

#define MIRROR 1       // set to 1 or -1
#define FASTLOAD 0     // don't load textures

#define MAX_COLORS 2
#define MAX_UVS 8
#define MAX_WEIGHTS 4

/****************************************************************************/
/***                                                                      ***/
/***                                                                      ***/
/***                                                                      ***/
/****************************************************************************/

namespace Wz4
{

  enum XSIAttributeType
  {
    AT_NONE,
    AT_POS,
    AT_NORMAL,
    AT_UV,
    AT_COLOR,
    AT_TANGENT,
    AT_WEIGHTMAP,
    AT_USERNORMAL,
  };

  struct XSIAttribute
  {
    sPoolString Name;
    sInt Type;
    sArray<sVector4> Data;
    sInt NormalIndex;

    XSIAttribute() { NormalIndex=-1; Type=AT_NONE; }
  };

  struct XSIMaterialTSpace
  {
    sPoolString Mesh;
    sPoolString TSpace;
    XSIMaterialTSpace() {}
    XSIMaterialTSpace(sPoolString mesh,sPoolString space) { Mesh=mesh; TSpace=space; }
  };

  struct XSIMaterialTexture
  {
    sPoolString Name;
    sArray<XSIMaterialTSpace> TSpace;
    sBool Enable;
    Texture2D *Tex;
  };

  struct XSIImage
  {
    sPoolString Name;
    sPoolString File;
    Texture2D *Texture_;
  };

  class XSIMaterial : public sObject
  {
  public:
    sCLASSNAME(XSIMaterial);
    XSIMaterial();
    ~XSIMaterial();
    void Tag();

    sPoolString Name;
    XSIMaterialTexture Textures[8];

    void Para(sPoolString shader,sPoolString para,sInt value);
    void Para(sPoolString shader,sPoolString para,sF32 value);
    void Para(sPoolString shader,sPoolString para,sPoolString value);
    void Connect(sPoolString shader,sPoolString para,sPoolString value);
    void TSpace(sPoolString shader,sPoolString para,sPoolString mesh,sPoolString value);
    Wz4Mtrl *MakeMaterial(class XSILoader *loader,sBool forceanim);
  };

  class XSIModel : public sObject
  {
  public:
    sCLASSNAME(XSIModel);
    XSIModel();
    ~XSIModel();
    void Tag();

    sInt JointId;
    sPoolString Name;
    Wz4Mesh *Mesh;
    sArray<sInt> PositionMap;
  };

  class XSILoader : public sObject
  {
    sScanner Scan;
    sPoolString Path;

    void ScanXSIName(sPoolString &str);   // name-name

    void ScanString(sPoolString &str);    // "string",
    sInt ScanInt();                       // 123,
    sF32 ScanFloat();                     // 1.23,
    void ScanVector(sVector30 &v);
    void ScanVector(sVector31 &v);
    void SyntaxError();
      
    void _DummySection();
    void _FCurve(Wz4ChannelPerFrame *channel);
    void _FCurveSkip();
    void _CustomPSet();
    void _Material();
    void _MaterialLibrary();
    void _Image();
    void _ImageLibrary();
    void _Mesh(XSIModel *model,sInt jointid,sBool animated);
    void _Model(sInt jid,sBool animated);
    void _EnvelopeList();
    void _Global();

    void ConnectMaterials();
    void FixAnimatedClusters();

    // work on data

  public:
    Texture2D *GetTexture(XSIImage *xi, sBool forcergb);

  private:
    // state of scanning

    XSIMaterial *DefaultMtrl;
    sArray<XSIMaterial *> Materials;
    sArray<XSIImage *> Images;
    sArray<XSIModel *> Models;
    sArray<Texture2D *> OutTextures;
    sArray<Wz4Mtrl *> OutMaterials;

    sMatrix34 Transform;
    sMatrix34 BasePose;
    Wz4Mesh *MasterMesh;
    sInt Visible;
    sArray<XSIAttribute *> Attributes;
    sBool MaterialsConnected;
  public:
    sCLASSNAME(XSILoader);
    XSILoader();
    ~XSILoader();
    void Tag();

    sBool Load(Wz4Mesh *master,const sChar *file);
    sBool ForceAnim;
    sBool ForceRGB;
  };

  /****************************************************************************/
  /***                                                                      ***/
  /***                                                                      ***/
  /***                                                                      ***/
  /****************************************************************************/

  XSIMaterial::XSIMaterial()
  {
    for(sInt i=0;i<sCOUNTOF(Textures);i++)
    {
      Textures[i].Tex = 0;
      Textures[i].Enable = 0;
    }
  }

  XSIMaterial::~XSIMaterial()
  {
    for(sInt i=0;i<sCOUNTOF(Textures);i++)
      sRelease(Textures[i].Tex);
  }

  void XSIMaterial::Tag()
  {
  }

  void XSIMaterial::Para(sPoolString shader,sPoolString para,sInt value)
  {
    if(shader==L"Softimage.OGLMulti.1" || shader==L"Softimage.OGLMulti.1.0")
    {
      if(para==L"Texture_1_Enable") Textures[0].Enable = value;
      else if(para==L"Texture_2_Enable") Textures[1].Enable = value;
      else if(para==L"Texture_3_Enable") Textures[2].Enable = value;
      else if(para==L"Texture_4_Enable") Textures[3].Enable = value;
    }
  }

  void XSIMaterial::Para(sPoolString shader,sPoolString para,sF32 value)
  {
  }

  void XSIMaterial::Para(sPoolString shader,sPoolString para,sPoolString value)
  {
    TSpace(shader,para,L"",value);
  }

  void XSIMaterial::Connect(sPoolString shader,sPoolString para,sPoolString value)
  {
    if(shader==L"Softimage.OGLMulti.1" || shader==L"Softimage.OGLMulti.1.0")
    {
      if(para==L"Texture_1") Textures[0].Name = value;
      else if(para==L"Texture_2") Textures[1].Name = value;
      else if(para==L"Texture_3") Textures[2].Name = value;
      else if(para==L"Texture_4") Textures[3].Name = value;
    }
  }

  void XSIMaterial::TSpace(sPoolString shader,sPoolString para,sPoolString mesh,sPoolString value)
  {
    if (value==L"") return;

    if(shader==L"Softimage.OGLMulti.1" || shader==L"Softimage.OGLMulti.1.0")
    {
      sInt stage = -1;
      if(para==L"tspace_id1") stage=0;
      if(para==L"tspace_id2") stage=1;
      if(para==L"tspace_id3") stage=2;
      if(para==L"tspace_id4") stage=3;
      if(stage!=-1)
      {
        Textures[stage].TSpace.AddTail(XSIMaterialTSpace(mesh,value));
      }
    }
    if(shader==L"Softimage.txt2d-image-explicit.1" || shader==L"Softimage.txt2d-image-explicit.1.0")
    {
      if(para==L"tspace_id")
      {
        Textures[0].TSpace.AddTail(XSIMaterialTSpace(mesh,value));
      }
    }
  }

  Wz4Mtrl *XSIMaterial::MakeMaterial(XSILoader *loader,sBool forceanim)
  {
    SimpleMtrl *mtrl;

    sInt count=0;
    for(sInt i=0;i<sCOUNTOF(Textures);i++)
      if(Textures[i].Tex)
        count++;

    mtrl = new SimpleMtrl;
    mtrl->Name = Name;

    sInt flags=sMTRL_ZON|sMTRL_CULLON;
    sInt extras=0;
    sInt blend=0;

    sBool hasdiff=sFALSE;
    sBool hasenvi=sFALSE;

    for(sInt i=0;i<sCOUNTOF(Textures);i++)
    {
      sBool envyhack = sFindString(Textures[i].Name,L"envy")>=0 && i>0;

      if(sFindString(Textures[i].Name,L"-diff")>=0 && !envyhack)
      {
        mtrl->SetTex(0,Textures[i].Tex);
        hasdiff=sTRUE;
      }

      if(sFindString(Textures[i].Name,L"-envi")>=0 || envyhack)
      {
        mtrl->SetTex(1,Textures[i].Tex);
        hasenvi=sTRUE;
        extras|=SimpleShader::ExtraDetailRefl;
        //sh->DetailMode = 0x51;
      }


      /*
      if(sFindString(Textures[i].Name,L"-norm")>=0)
        mtrl->SetTex(0,Textures[i].Tex);
      if(sFindString(Textures[i].Name,L"-diff")>=0 && !envyhack)
      {
        mtrl->SetTex(1,Textures[i].Tex);
        hasdiff=sTRUE;
      }
      if(sFindString(Textures[i].Name,L"-spec")>=0)
      {
        mtrl->SetTex(2,Textures[i].Tex);
        hasenvi=sTRUE;
        //sh->DetailMode = 4;
      }
      */
    }

    if(hasenvi && !hasdiff /*&& sh->DetailMode == 0x51*/) // envi, but no diffuse?
    {
      // add dummy 1x1 pixel white texture as diffuse layer
      XSIImage xi;
      xi.File = L"__white__gen ";
      xi.Name = L"constant_white";
      xi.Texture_ = 0;
      mtrl->SetTex(0, loader->GetTexture(&xi,loader->ForceRGB));
    }

    mtrl->SetMtrl(flags,blend,extras);
    mtrl->Prepare();

    if(1) // print some stats)
    {
      sDPrintF(L"* Material %q <",mtrl->Name);
      for(sInt i=0;i<8;i++)
        if(Textures[i].Tex) 
          sDPrintF(L"%q,",Textures[i].Name);
      sDPrintF(L">\n");
      /*
      for(sInt i=0;i<8;i++)
      {
        if(mtrl->Tex[i])
          sDPrintF(L" [%d] %q\n",i,((Texture2D *)mtrl->Tex[i])->Name);
      }
      */
    }



    return mtrl;
  }

  /****************************************************************************/
  /***                                                                      ***/
  /***                                                                      ***/
  /***                                                                      ***/
  /****************************************************************************/

  XSIModel::XSIModel()
  {
    Mesh = 0;
    JointId = -1;
  }

  XSIModel::~XSIModel()
  {
    sRelease(Mesh);
  }

  void XSIModel::Tag()
  {
  }

  /****************************************************************************/
  /***                                                                      ***/
  /***                                                                      ***/
  /***                                                                      ***/
  /****************************************************************************/

  XSILoader::XSILoader()
  {
    DefaultMtrl = 0;
  }

  XSILoader::~XSILoader()
  {    
    sDeleteAll(Images);
  }

  void XSILoader::Tag()
  {
    DefaultMtrl->Need();
    sNeed(Materials);
    sNeed(Models);
  }

  /****************************************************************************/

  void XSILoader::ScanXSIName(sPoolString &str)
  {
    sPoolString name;
    sString<256> buffer;


    Scan.ScanName(name);
    buffer = name;

    while(Scan.Token=='-' || Scan.Token=='.')
    {
      if(Scan.IfToken('-'))
      {
        buffer.Add(L"-");
        Scan.ScanName(name);
        buffer.Add(name);
      }
      else if(Scan.IfToken('.'))
      {
        buffer.Add(L".");
        Scan.ScanName(name);
        buffer.Add(name);
      }
    }

    str = buffer;
  }

  void XSILoader::ScanString(sPoolString &str)
  {
    Scan.ScanString(str);
    Scan.Match(',');
  }

  sInt XSILoader::ScanInt()
  {
    sInt result,sign;

    sign = 1;
    if(Scan.IfToken('-'))
      sign = -1;
    result = Scan.ScanInt();
    Scan.Match(',');

    return result*sign;
  }

  sF32 XSILoader::ScanFloat()
  {
    sF32 result,sign;

    sign = 1;
    if(Scan.IfToken('-'))
      sign = -1;
    result = Scan.ScanFloat();
    Scan.Match(',');

    return result*sign;
  }

  void XSILoader::ScanVector(sVector30 &v)
  {
    v.x = ScanFloat();
    v.y = ScanFloat();
    v.z = ScanFloat();
  }

  void XSILoader::ScanVector(sVector31 &v)
  {
    v.x = ScanFloat();
    v.y = ScanFloat();
    v.z = ScanFloat();
  }

  void XSILoader::SyntaxError()
  {
    Scan.Error(L"syntax error");
  }

  /****************************************************************************/
  /****************************************************************************/

  void XSILoader::_DummySection()
  {
    sPoolString dummy;
    if(Scan.Token==sTOK_NAME)
      Scan.ScanName(dummy);
    Scan.ScanRaw(dummy,'{','}');
  }

  void XSILoader::_FCurve(Wz4ChannelPerFrame *channel)
  {
    sPoolString dummy,component,interpolation;
    sInt component_kind;
    sInt component_dim;
    sF32 *dp;//,*fp;

    Scan.Match('{');
    ScanString(dummy);
    ScanString(component);
    ScanString(interpolation);

    sInt curves = ScanInt();
    sInt values = ScanInt();
    sInt keys = ScanInt();

    component_kind = component_dim = 0;
    if(component==L"sclx") { component_kind = 1; component_dim = 0; }
    if(component==L"scly") { component_kind = 1; component_dim = 1; }
    if(component==L"sclz") { component_kind = 1; component_dim = 2; }
    if(component==L"rotx") { component_kind = 2; component_dim = 0; }
    if(component==L"roty") { component_kind = 2; component_dim = 1; }
    if(component==L"rotz") { component_kind = 2; component_dim = 2; }
    if(component==L"posx") { component_kind = 3; component_dim = 0; }
    if(component==L"posy") { component_kind = 3; component_dim = 1; }
    if(component==L"posz") { component_kind = 3; component_dim = 2; }
    if(component==L"pcrotx") { component_kind = -1; component_dim = 0; }
    if(component==L"pcroty") { component_kind = -1; component_dim = 1; }
    if(component==L"pcrotz") { component_kind = -1; component_dim = 2; }
    if(component_kind==0) Scan.Error(L"unknown component <%s>",component);

    dp = 0;
    if(channel && values>=1)
    {
      if(channel->Keys==0)
      {
        channel->Keys=keys;
      }
      else if(channel->Keys!=keys)
      {
        Scan.Error(L"fcurve components defined with different key count (%d,%d)",channel->Keys,keys);
  //      sDPrintF(L"fcurve components defined with different key count (%d,%d)\n",channel->Keys,keys);
      }
      switch(component_kind)
      {
      case 1:
        if(channel->Scale == 0)
        {
          channel->Scale = new sVector31[channel->Keys];
          for(sInt i=0;i<channel->Keys;i++)
            channel->Scale[i] = channel->Start.Scale;
        }
        dp = &channel->Scale[0].x + component_dim;
        break;
      case 2:
        if(channel->User == 0)
        {
          channel->User = new sVector31[channel->Keys];
          for(sInt i=0;i<channel->Keys;i++)
            channel->User[i] = channel->Start.User;
        }
        dp = &channel->User[0].x + component_dim;
        break;
      case 3:
        if(channel->Trans == 0)
        {
          channel->Trans = new sVector31[channel->Keys];
          for(sInt i=0;i<channel->Keys;i++)
            channel->Trans[i] = channel->Start.Trans;
        }
        dp = &channel->Trans[0].x + component_dim;
        break;
      }
    }

    //  fp = dp;
  //  if(channel->Keys!=keys)     // bug!
  //    dp =0;
    if(curves!=1)
      Scan.Error(L"can't handle curves != 1");

    sF32 val;
    for(sInt i=0;i<keys;i++)
    {
      ScanFloat();
      val = ScanFloat();
      for(sInt j=1;j<values;j++)
        ScanFloat();
      if(dp)
      {
        *dp = val;
        dp += 3;
      }
    }
    Scan.Match('}');
  }

  void XSILoader::_FCurveSkip()
  {
    sPoolString dummy;

    Scan.Match('{');
    ScanString(dummy);
    ScanString(dummy);
    ScanString(dummy);

    sInt curves = ScanInt();
    sInt values = ScanInt();
    sInt keys = ScanInt();
    if(curves!=1)
      Scan.Error(L"can't handle curves != 1");

    for(sInt i=0;i<keys;i++)
    {
      ScanFloat();
      for(sInt j=0;j<values;j++)
        ScanFloat();
    }
    Scan.Match('}');
  }

  void XSILoader::_CustomPSet()
  {
    sPoolString dummy;
    sPoolString para,type,value;
    sF32 valf;
    sInt vali;

    ScanXSIName(dummy);
    Scan.Match('{');
    ScanString(dummy);
    ScanString(dummy);
    sInt max = ScanInt();
    for(sInt i=0;i<max;i++)
    {
      ScanString(para);
      ScanString(type);
      value = L"";
      if(type==L"Boolean" || type==L"Small Integer Number" || type==L"Integer")
      {
        valf = vali = ScanInt();
      }
      else if(type==L"Floating Point Number")
      {
        valf = ScanFloat();
        vali = sInt(valf);
      }
      else if(type==L"Text")
      {
        valf = vali = 0;
        ScanString(value);
      }
      else
      {
        Scan.Error(L"unexpected type %s in XSI_CustomPSet",type);
      }

      while(Scan.IfName(L"XSI_CustomParamInfo"))
      {
        ScanXSIName(dummy);
        Scan.Match('{');
        ScanFloat();
        ScanFloat();
        ScanInt();
        Scan.Match('}');
      }

      while(Scan.IfName(L"SI_FCurve"))
      {
        _FCurveSkip();
      }
    }
    Scan.Match('}');
  }

  /****************************************************************************/

  void XSILoader::_Material()
  {
    sPoolString dummy,name;
    sPoolString para,type,value;
    sPoolString bestshader,surfaceshader,realtimeshader;
    sInt vali;
    sF32 valf;
    sString<256> tmpstr;


    XSIMaterial *mtrl = new XSIMaterial;
    Materials.AddTail(mtrl);

    ScanXSIName(mtrl->Name);
    sDPrintF(L"found material %q\n",mtrl->Name);
    Scan.Match('{');

    // what shader is best for picking textures?

    sInt bindings = ScanInt();
    for(sInt i=0;i<bindings;i++)
    {
      ScanString(para);     // bind to
      ScanString(value);     // what
      bestshader = value;
      if(para==L"surface") surfaceshader = value;
      if(para==L"RealTime") realtimeshader = value;
    }

    if(!surfaceshader.IsEmpty()) bestshader = surfaceshader;      // surface shader is not bad
    if(!realtimeshader.IsEmpty()) bestshader = realtimeshader;    // but prefere realtimeshader

    while(Scan.Token!='}' && !Scan.Errors)
    {
      if(Scan.IfName(L"XSI_MaterialInfo"))
      {
        Scan.Match('{');
        ScanInt();    // wrapu
        ScanInt();    // wrapv
        Scan.Match('}');
      }
      else if(Scan.IfName(L"XSI_Shader"))
      {
        ScanXSIName(dummy);
  //      sDPrintF(L"shader %q\n",shader);

        sBool goodshader = (sCmpStringILen(dummy,L"OGL",3)==0);

        Scan.Match('{');
        sPoolString shader;
        ScanString(shader);
        /*sInt outs =*/ ScanInt();
        sInt paras = ScanInt();
        sInt connects = ScanInt();

        for(sInt i=0;i<paras;i++)
        {
          ScanString(para);
          ScanString(type);
          value=L"";
          if(type==L"BOOLEAN" || type==L"BYTE" || type==L"INTEGER")
          {
            vali = ScanInt();
            if(goodshader) mtrl->Para(shader,para,vali);
          }
          else if(type==L"FLOAT")
          {
            valf = ScanFloat();
            if(goodshader) mtrl->Para(shader,para,valf);
          }
          else if(type==L"STRING" || type==L"TEXT")
          {
            ScanString(value);
            if(goodshader) mtrl->Para(shader,para,value);
          }
          else
          {
            Scan.Error(L"unknown type %s in material parameters",type);
          }
        }
        for(sInt i=0;i<connects;i++)
        {
          ScanString(para);
          ScanString(value);
          ScanString(type);

          if(goodshader) mtrl->Connect(shader,para,value);
        }
        while(Scan.Token!='}' && !Scan.Errors)
        {
          if(Scan.IfName(L"XSI_ShaderInstanceData"))
          {
            sPoolString dummy;
            Scan.Match('{');
            ScanString(dummy);
            tmpstr.PrintF(L"MDL-%s",dummy);
            dummy=tmpstr;
            sInt max = ScanInt();

            for(sInt i=0;i<max;i++)
            {
              ScanString(para);
              ScanString(type);
              ScanString(value);
              if(goodshader) mtrl->TSpace(shader,para,dummy,value);
            }
            Scan.Match('}');
          }
          else if(Scan.IfName(L"XSI_CustomPSet"))
          {
            sPoolString name;
            Scan.ScanName(name);
            Scan.ScanRaw(name,'{','}');
          }
          else if(Scan.IfName(L"SI_FCurve"))
          {
            _FCurveSkip();
          }
          else if(Scan.IfName(L"XSI_ShaderMultiPortConnection"))
          {
            Scan.Match('{');
            ScanString(dummy);
            ScanString(dummy);
            Scan.Match('}');
          }
          else
          {
            Scan.Error(L"XSI_ShaderInstanceData expected");
          }
        }
        Scan.Match('}');
      }
      else if(Scan.IfName(L"XSI_ShaderMultiPortConnection"))
      {
        Scan.Match('{');
        sPoolString dummy;
        ScanString(dummy);
        ScanString(dummy);
        Scan.Match('}');
      }
      else 
      {
        Scan.Error(L"XSI_Shader expected");
      }
    }
    Scan.Match('}');
  }

  void XSILoader::_MaterialLibrary()
  {
    if(Scan.Token==sTOK_NAME)
    {
      sPoolString dummy;
      ScanXSIName(dummy);
    }

    Scan.Match('{');
    sInt max = ScanInt();
    for(sInt i=0;i<max;i++)
    {
      if(Scan.IfName(L"XSI_Material"))
      {
        _Material();
      }
      else
      {
        Scan.Error(L"XSI_Material expected");
      }
    }
    Scan.Match('}');
  }

  /****************************************************************************/

  static void CleanName(const sChar *s,const sStringDesc &dest)
  {
    sChar *d = dest.Buffer;
    const sChar *ss = s;
    
    sVERIFY(sGetStringLen(s)<dest.Size);

    ss = s;
    while(*ss)
    {
      if(*ss=='/' || *ss=='\\')
        s = ss+1;
      ss++; 
    }

    while(*s && *s!='.')
      *d++ = *s++;
    *d++ =0;
  }


  Texture2D *XSILoader::GetTexture(XSIImage *xi, sBool forcergb)
  {
    if(xi->Texture_==0)
    {
      sString<sMAXPATH> buffer;
      CleanName(xi->File,buffer);
      xi->Texture_ = sFind(OutTextures,&Texture2D::Name,(const sChar *)buffer);
    }

    if(xi->Texture_==0)
    {
      sString<sMAXPATH> buffer;
      buffer = Path;
      buffer.AddPath(sFindFileWithoutPath(xi->File));

      Texture2D *tex = new Texture2D;
      sImage img;
  #if FASTLOAD
      img.Init(64,64);
      img.Checker(0xffc0c0a0,0xffa0c0c0,8,8);
  #else
      if(xi->File == L"__white__gen ")
      {
        img.Init(1,1);
        img.Fill(0xffffffff);
      }
      else if(!img.Load(buffer))
      {
        *sFindFileExtension(buffer)=0; buffer.Add(L"pic"); if(img.Load(buffer)) goto ok;
        *sFindFileExtension(buffer)=0; buffer.Add(L"tga"); if(img.Load(buffer)) goto ok;
        *sFindFileExtension(buffer)=0; buffer.Add(L"bmp"); if(img.Load(buffer)) goto ok;
        img.Init(16,16);
        img.Fill(0xffff0000);
      ok:;
      }
  #endif
      CleanName(xi->File,tex->Name);
      sInt format = sTEX_DXT1;
      if(img.HasAlpha())
        format = sTEX_DXT5;
      if(forcergb || sFindString(buffer,L"-norm_")>=0)
        format = sTEX_ARGB8888;
      tex->ConvertFrom(&img,format|sTEX_2D);
      OutTextures.AddTail(tex);
      xi->Texture_ = tex;
    }
    return xi->Texture_;
  }



  void XSILoader::_Image()
  {
    sPoolString name,file;

    ScanXSIName(name);
    Scan.Match('{');
    ScanString(file);
    ScanInt();
    ScanInt();
    ScanInt();
    ScanInt();
    ScanFloat();
    ScanFloat();
    ScanFloat();
    ScanFloat();
    ScanFloat();
    ScanInt();
    ScanInt();
    ScanInt();

    if(!Scan.Errors)
    {
      XSIImage *xi = new XSIImage;
      xi->Name = name;
      xi->File = file;
      xi->Texture_ = 0;
      Images.AddTail(xi);
    }

    while(Scan.Token!='}' && Scan.Errors==0)
    {
      if(Scan.IfName(L"XSI_ImageFX"))
      {
        _DummySection();
      }
      else if(Scan.IfName(L"XSI_ImageFX2"))
      {
        _DummySection();
      }
      else if(Scan.IfName(L"XSI_TimeControl"))
      {
        _DummySection();
      }
      else
      {
        Scan.Error(L"Unexpected token");
      }
    }
    Scan.Match('}');
  }

  void XSILoader::_ImageLibrary()
  {
    Scan.Match('{');
    sInt max = ScanInt();
    for(sInt i=0;i<max;i++)
    {
      if(Scan.IfName(L"XSI_Image"))
      {
        _Image();
      }
      else
      {
        Scan.Error(L"XSI_Image expected");
      }
    }
    Scan.Match('}');
  }
    // conect materials to textures

  void XSILoader::ConnectMaterials()
  {
    if(MaterialsConnected)
      return;

    MaterialsConnected = 1;

    XSIMaterial *xm;
    XSIImage *xi;
    sFORALL(Materials,xm)
    {
      for(sInt i=0;i<sCOUNTOF(xm->Textures);i++)
      {
        if(!xm->Textures[i].Name.IsEmpty())
        {
          xi = sFind(Images,&XSIImage::Name,xm->Textures[i].Name);
          if(!xi)
          {
            Scan.Error(L"could not find texture <%s> in material <%s>",xm->Textures[i].Name,xm->Name);
          }
          else
          {
            xm->Textures[i].Tex = GetTexture(xi,ForceRGB);
            xm->Textures[i].Tex->AddRef();
          }
        }
      }
      if(!Scan.Errors)
      {
        Wz4Mtrl *mtrl = xm->MakeMaterial(this,ForceAnim);
        if(mtrl)
          OutMaterials.AddTail(mtrl);
        else
          Scan.Error(L"failed to create material <%s>",xm->Name);

        if(!ForceAnim && !sCheckSuffix(xm->Name,L"-anim")) // also create animated version of material
        {
          sPoolString origName = xm->Name;
          sString<256> name = xm->Name;
          sAppendString(name,L"-anim");

          xm->Name = name;
          Wz4Mtrl *mtrl = xm->MakeMaterial(this,ForceAnim);
          if(mtrl)
            OutMaterials.AddTail(mtrl);
          else
            Scan.Error(L"failed to create material <%s>",xm->Name);

          xm->Name = origName;
        }
      }
    }
  }

  /****************************************************************************/

  void XSILoader::_Mesh(XSIModel *model,sInt jointid,sBool animated)
  {
    sPoolString dummy;
    sPoolString name;
    sPoolString meshname;
    sTextBuffer tb;
    sInt max;
    XSIAttribute *at;

    ScanXSIName(meshname);
    Scan.Match('{');

    sVERIFY(Attributes.GetCount()==0);

    Wz4Mesh *mesh=new Wz4Mesh;

    sBool hasNormals=sFALSE;
    sBool hasTangents=sFALSE;

    // shape

    Scan.MatchName(L"XSI_Shape");
    ScanXSIName(dummy);
    mesh->Name = dummy;
    Scan.Match('{');
    ScanString(dummy);
    if(dummy!=L"ORDERED")
      Scan.Error(L"shape must be ORDERED");
    while(Scan.IfName(L"XSI_SubComponentAttributeList"))
    {
      at = new XSIAttribute;
      sPoolString usage,type;
      ScanXSIName(at->Name);
      Scan.Match('{');
      ScanString(usage);
      ScanString(type);
      max = ScanInt();
      at->Data.AddMany(max);
      Attributes.AddTail(at);

      if(usage==L"POSITION" && type==L"FLOAT3")
      {
        at->Type= AT_POS;
        for(sInt i=0;i<max;i++)
        {
          at->Data[i].x = ScanFloat(); 
          at->Data[i].y = ScanFloat(); 
          at->Data[i].z = ScanFloat()*MIRROR;
          at->Data[i].w = 1;
        }
      }
      else if(usage==L"NORMAL" && type==L"FLOAT3")
      {
        at->Type= AT_NORMAL;
        if(at->Name==L"User_Normal")
          at->Type = AT_USERNORMAL;
        for(sInt i=0;i<max;i++)
        {
          at->Data[i].x = ScanFloat(); 
          at->Data[i].y = ScanFloat(); 
          at->Data[i].z = ScanFloat()*MIRROR;
          at->Data[i].w = 0;
        }
        hasNormals=sTRUE;
      }
      else if(usage==L"TEXCOORD" && type==L"FLOAT2")
      {
        at->Type= AT_UV;
        for(sInt i=0;i<max;i++)
        {
          at->Data[i].x = ScanFloat(); 
          at->Data[i].y = ScanFloat(); 
          at->Data[i].z = 0;
          at->Data[i].w = 0;
        }
      }
      else if(usage==L"TEXTANGENT" && type==L"FLOAT4")
      {
        at->Type= AT_TANGENT;
        for(sInt i=0;i<max;i++)
        {
          at->Data[i].x = ScanFloat(); 
          at->Data[i].y = ScanFloat(); 
          at->Data[i].z = ScanFloat()*MIRROR; 
          at->Data[i].w = ScanFloat(); 
        }
        hasTangents=sTRUE;
      }
      else if(usage==L"COLOR" && type==L"FLOAT4")
      {
        at->Type= AT_COLOR;
        for(sInt i=0;i<max;i++)
        {
          at->Data[i].x = ScanFloat(); 
          at->Data[i].y = ScanFloat(); 
          at->Data[i].z = ScanFloat();
          at->Data[i].w = ScanFloat();
        }
      }
      else if(usage==L"WEIGHTMAP" && type==L"FLOAT")
      {
        at->Type= AT_WEIGHTMAP;
        for(sInt i=0;i<max;i++)
        {
          at->Data[i].x = ScanFloat(); 
        }
      }
      else
      {
        Scan.Error(L"unknown XSI_SubComponentAttributeList %s / %s",usage,type);
      }

      Scan.Match('}');
    }
    Scan.Match('}');

    // vertex

    Scan.MatchName(L"XSI_VertexList");
    Scan.Match('{');
    sInt attribues = ScanInt();

    ScanString(name);                 // first attribute: position
    for(sInt i=1;i<attribues;i++)     // remaining attributeS: weightmaps, don't care
      ScanString(dummy);

    XSIAttribute *pa = sFind(Attributes,&XSIAttribute::Name,name);
    if(!pa)
      Scan.Error(L"could not find XSIAttribute %s");
    if(pa->Type!=AT_POS)
      Scan.Error(L"attribute %s should be of type position");
    
    if(Scan.Errors) return;
    
    max = ScanInt();

    sArray<sInt> positionmap;
    positionmap.AddMany(max);

    for(sInt i=0;i<max && !Scan.Errors;i++)
    {
      sInt index = ScanInt();
      for(sInt j=1;j<attribues;j++)
        ScanInt();

      if(index<0 || index>=at->Data.GetCount())
        Scan.Error(L"attribute out of range");
      else
        positionmap[i] = index;
    }
    Scan.Match('}');

    // polygon list & attributes

    while(!Scan.Errors)
    {
      sInt poly = 0;
      if(Scan.IfName(L"XSI_PolygonList"))
      {
        poly = 1;
      }
      else if(Scan.IfName(L"XSI_TriangleList"))
      {
        poly = 0;
      }
      else
      {
        break;
      }

      Wz4MeshCluster *cl = new Wz4MeshCluster();
      sInt cli = mesh->Clusters.GetCount();
      mesh->Clusters.AddTail(cl);

      // what attributes do we have?
      XSIAttribute *na;
      XSIAttribute *tanga;
      XSIAttribute *ca[MAX_COLORS];
      XSIAttribute *ta[MAX_UVS];
      sInt ni,tangi;
      sInt ci[MAX_COLORS];
      sInt ti[MAX_UVS];

      sInt cc=0;
      sInt tc=0;

      na = 0;
      tanga = 0;
      ni = 0;
      tangi = 0;
      for(sInt i=0;i<MAX_COLORS;i++)
        ca[i] = 0;
      for(sInt i=0;i<MAX_UVS;i++)
        ta[i] = 0;

      ScanXSIName(dummy);
      Scan.Match('{');

      sInt elements = ScanInt();
      for(sInt i=0;i<elements;i++)
      {
        ScanString(name);
        at = sFind(Attributes,&XSIAttribute::Name,name);
        if(at)
        {
          switch(at->Type)
          {
          case AT_USERNORMAL:
            // ignore
            break;
          case AT_NORMAL:
            if(na)
            {
              Scan.Error(L"too many normals");
            }
            else
            {
              na = at;
              ni = i+1;
            }
            break;

          case AT_TANGENT:
            if(tanga)
            {
              Scan.Error(L"too many tangents");
            }
            else
            {
              tanga = at;
              tangi = i+1;
            }
            break;

          case AT_COLOR:
            if(cc==MAX_COLORS)
            {
              Scan.Error(L"too many colors");
            }
            else
            {
              ca[cc] = at;
              ci[cc] = i+1;
              cc++;
            }
            break;

          case AT_UV:
            if(tc==MAX_UVS)
            {
              Scan.Error(L"too many uv's in .xsi");
            }
            else
            {
              ta[tc] = at;
              ti[tc] = i+1;
              tc++;
            }
            break;

          default:
            Scan.Error(L"unexpected attribute %s",name);
            break;
          }
        }
        else
        {
          Scan.Error(L"unknown attribute %s",name);
        }
      }
      if(na==0)
      {
        sDPrintF(L"SI_MESH %s: no normals found\n",meshname);
  //      Scan.Error(L"no normals found");
      }

      // material

      ScanString(name);
      sVERIFY(cl->Mtrl==0);
      sPoolString mtrlName = name;

      XSIMaterial *mtrl = sFind(Materials,&XSIMaterial::Name,mtrlName);
      if(mtrl==0)
      {
        mtrlName = name = L"***Default";
        if(DefaultMtrl == 0)
        {
          DefaultMtrl = new XSIMaterial;
          DefaultMtrl->Name = name;
          SimpleMtrl *wm = new SimpleMtrl;
          wm->SetMtrl(sMTRL_ZON|sMTRL_CULLON,0,0);
          wm->Prepare();
          wm->Name = DefaultMtrl->Name;
          OutMaterials.AddTail(wm);
        }
        mtrl = DefaultMtrl;
      }

      if(animated && !ForceAnim && !sCheckSuffix(name,L"-anim"))
      {
        sString<256> buffer = name;
        sAppendString(buffer,L"-anim");
        mtrlName = buffer;
      }
      cl->Mtrl = sFind(OutMaterials,&Wz4Mtrl::Name,mtrlName);
      if(!cl->Mtrl)
        Scan.Error(L"material <%s> not found",name);
      else
        cl->Mtrl->AddRef();

      // hier sollte der code sein, der die texture spaces vermischt
      // wir brauchen also auf alle f�lle einen shader!

      sInt tspaces[2]={-1,-1};
      sInt ntspaces=0;

      for (sInt i=0; i<sCOUNTOF(mtrl->Textures); i++) // if (mtrl->Textures[i].Enable)
      {
        sBool found=sFALSE;
        XSIMaterialTSpace *ts;
        sFORALLREVERSE(mtrl->Textures[i].TSpace,ts)
        {
          if (ts->Mesh==L"" || !sCmpStringI(ts->Mesh,model->Name))
          {
            found=sTRUE;
            break;
          }
        }

        if (!found)
        {
          if (Visible)
            sDPrintF(L"warning: no texturespace found for model %s / mtrl %s / tex %s\n",model->Name,mtrl->Name,mtrl->Textures[i].Name);
//          if (!ntspaces)                // CHAOS: i removed these lines because they cause crashes, but i don't know if that breaks something
//            tspaces[ntspaces++]=0;
          break;
        }

        found=sFALSE;
        for (sInt j=0; j<ntspaces; j++)
          if (ta[tspaces[j]]->Name==ts->TSpace)
            found=sTRUE;
        if (found) break;
        if (ntspaces==sCOUNTOF(tspaces))
        {
          Scan.Error(L"too many actual uvs in cluster");
          break;
        }
        for (sInt j=0; j<tc; j++)
          if (ta[j]->Name==ts->TSpace)
          {
            found=sTRUE;
            tspaces[ntspaces++]=j;
            break;
          }

        if (!found)
        {
          if (Visible) 
            sDPrintF(L"warning: no texturespace found for model %s / mtrl %s / tex %s\n",model->Name,mtrl->Name,mtrl->Textures[i].Name);
          if (!ntspaces)
            tspaces[ntspaces++]=0;
          break;
        }
      }
     
      if(poly)
      {
        /*sInt vertexmax =*/ ScanInt();
      }
      sInt polygons = ScanInt();

      // start building cluster

      for(sInt i=0;i<polygons;i++)
      {
        sInt vb[16];
        sInt vertices = 3;
        if(poly)
          vertices = ScanInt();
        sVERIFY(vertices<256);
        sVERIFY(elements<15);

        sInt vi=mesh->Vertices.GetCount();
        Wz4MeshVertex *vp=mesh->Vertices.AddMany(vertices);
        sInt *pmp=model->PositionMap.AddMany(vertices);

        for(sInt i=0;i<vertices;i++)
        {
          for(sInt j=0;j<elements+1;j++)
            vb[j]=ScanInt();

          Wz4MeshVertex &v=vp[i];            
          v.Init();
          
          sInt posindex=positionmap[vb[0]];
          pmp[i]=posindex;
          const sVector4 &pos=pa->Data[posindex];
          v.Pos.x=pos.x; v.Pos.y=pos.y; v.Pos.z=pos.z;

          if (na)
          {
            const sVector4 &norm=na->Data[vb[ni]];
            v.Normal.x=norm.x; v.Normal.y=norm.y; v.Normal.z=norm.z;
          }

          if (tanga)
          {
            const sVector4 &tangent=tanga->Data[vb[tangi]];
            v.Tangent.x=tangent.x; v.Tangent.y=tangent.y; v.Tangent.z=tangent.z;
            v.BiSign=tangent.w;
          }

#if !WZ4MESH_LOWMEM
          if (ca[0])
            v.Color0=ca[0]->Data[vb[ci[0]]].GetColor();

          if (ca[1])
            v.Color0=ca[1]->Data[vb[ci[1]]].GetColor();
#endif

          if (tspaces[0]>=0)
          {
            sVector4 &uv=ta[tspaces[0]]->Data[vb[ti[tspaces[0]]]];
            v.U0=uv.x; v.V0=1.0f-uv.y;
          }

          if (tspaces[1]>=0)
          {
            sVector4 &uv=ta[tspaces[1]]->Data[vb[ti[tspaces[1]]]];
            v.U1=uv.x; v.V1=1.0f-uv.y;
          }
        }

        if(vertices<=4)
        {
          Wz4MeshFace *face = mesh->Faces.AddMany(1);
          face->Init(vertices);
          face->Cluster = cli;

          for(sInt i=0;i<vertices;i++)
          {
            if(MIRROR==-1)
              face->Vertex[vertices-1-i]=vi+i;
            else
              face->Vertex[i]=vi+i;
          }
        }
        else
        {
          Wz4MeshFace *face = mesh->Faces.AddMany(vertices-2);
          for(sInt i=2;i<vertices;i++)
          {
            face->Init(3);
            face->Cluster = cli;

            if(MIRROR==-1)
            {
              face->Vertex[0]=vi;
              face->Vertex[1]=vi+i-1;
              face->Vertex[2]=vi+i;
            }
            else
            {
              face->Vertex[2]=vi;
              face->Vertex[1]=vi+i-1;
              face->Vertex[0]=vi+i;
            }
            face++;
          }
        }
      }

      if(Scan.IfName(L"XSI_IndexList"))   // ignore this...
      {
        Scan.Match('{');
        sInt n = ScanInt();
        for(sInt i=0;i<n;i++)
          ScanInt();
        Scan.Match('}');
      }

      Scan.Match('}');
    }

    // done

    Scan.Match('}');

    if (!hasNormals) mesh->CalcNormals();
    if (!hasTangents) mesh->CalcTangents();

    if(Visible)
    {
      model->Mesh=mesh;
      mesh->AddRef();
    }
    sRelease(mesh);

    sDeleteAll(Attributes);
  }

  void XSILoader::_Model(sInt jointid,sBool animated)
  {
    sTextBuffer tb;
    sPoolString dummy;
    sInt visible = 1;

    XSIModel *model = new XSIModel;
    ScanXSIName(model->Name);
    model->JointId=jointid;

    ConnectMaterials();

    Wz4ChannelPerFrame *channel = new Wz4ChannelPerFrame;
    Wz4AnimJoint *joint = MasterMesh->Skeleton->Joints.AddMany(1);
    joint->Init();
    joint->Channel = channel;
    joint->Parent = jointid; jointid = MasterMesh->Skeleton->Joints.GetCount()-1;
    joint->Name = model->Name;

    Scan.Match('{');
    while(!Scan.Errors && Scan.Token!='}')
    {
      if(Scan.IfName(L"SI_Visibility"))
      {
        Scan.Match('{');
        visible = ScanInt();
        Scan.Match('}');
      }
      else if(Scan.IfName(L"XSI_Transform"))
      {
        sVector31 s;
        sVector30 r;
        sVector31 t;

        // figure out matrices

        Scan.Match('{');
        ScanVector(t);
        ScanVector(r);
        ScanString(dummy);
        ScanVector(s);
        sInt siscaling = ScanInt();
        if(dummy!=L"XYZ")
          sDPrintF(L"XSI_Transform %s: coordinate system wrong (%s)\n",model->Name,dummy);
        if(siscaling && (s.x!=s.y || s.x!=s.z))
          sDPrintF(L"XSI_Transform %s: softimage scaling with different scaling in x/y/z not supported (%f)\n",model->Name,s);

  //      sDPrintF(L"%d %f : %s\n",siscaling,s,model->Name);

        static sInt match[] = { 0,0,0, 0,0,0, 0,0,0, 1,1,1, 0,0,0, 0,0,0, 1,1,1, 1,1,1, 0,0,0, 0,0,0, 0,0,0 };
        for(sInt i=0;i<sCOUNTOF(match);i++)
        {
          if(match[i]!=sInt(ScanFloat()))
          {
  //          Scan.Error(L"XSI_Transform %s has unexpected data",model->Name);
            sDPrintF(L"XSI_Transform %s has unexpected data\n",model->Name);
          }
        }

        sMatrix34 mat,mat0;
        mat.EulerXYZ(r.x*sPI2F/360,r.y*sPI2F/360,r.z*sPI2F/360);
        channel->Start.User.Init(r.x,r.y,r.z);
        channel->Start.Rot.Init(mat);
        channel->Start.Scale = s;
        channel->Start.Trans = t;

        mat.l = t;
        mat.i.x *= s.x;
        mat.i.y *= s.y;
        mat.i.z *= s.z;
        mat.j.x *= s.x;
        mat.j.y *= s.y;
        mat.j.z *= s.z;
        mat.k.x *= s.x;
        mat.k.y *= s.y;
        mat.k.z *= s.z;

        mat.i.z *= MIRROR;
        mat.j.z *= MIRROR;
        mat.k.x *= MIRROR;
        mat.k.y *= MIRROR;
        mat.l.z *= MIRROR;

        mat0 = Transform;
        Transform = mat * mat0;
        joint->fake = Transform;

        // skip limits

        while(Scan.IfName(L"XSI_Limit"))
        {
          Scan.Match('{');
          ScanString(dummy);
          ScanInt(); ScanFloat();
          ScanInt(); ScanFloat();
          Scan.Match('}');
        }

        // scan fcurves

        while(Scan.IfName(L"SI_FCurve"))
        {
          animated = 1;
          _FCurve(channel);
        }
        Scan.Match('}');

        // convert euler to quaternion

        if(channel->User)
        {
          channel->Rot = new sQuaternion[channel->Keys];
          for(sInt i=0;i<channel->Keys;i++)
          {
            mat.EulerXYZ(channel->User[i].x*sPI2F/360,channel->User[i].y*sPI2F/360,channel->User[i].z*sPI2F/360);
            channel->Rot[i].Init(mat);
          }
          sDeleteArray(channel->User);
        }
      }
      else if(Scan.IfName(L"SI_GlobalMaterial"))
      {
        Scan.Match('{');
        ScanString(dummy);
        ScanString(dummy);
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_Null"))
      {
        ScanXSIName(dummy);
        Scan.Match('{');
        Scan.Match('}');
      }
      else if(Scan.IfName(L"XSI_BasePose"))
      {
        sVector31 s;
        sVector30 r;
        sVector31 t;

        Scan.Match('{');
        ScanVector(t);
        ScanVector(r); r*=sPI2F/360.0f;
        ScanVector(s);
        Scan.Match('}');

        if(s.x!=s.y || s.x!=s.z)
          sDPrintF(L"XSI_Basepose %s has directed scaling\n",model->Name);

        BasePose.l.Init(0,0,0);
        BasePose.EulerXYZ(r.x,r.y,r.z);
        BasePose.Trans3();
        BasePose.Scale(1/s.x,1/s.y,1/s.z);
        BasePose.l = -t*BasePose;

        BasePose.i.z *= MIRROR;
        BasePose.j.z *= MIRROR;
        BasePose.k.x *= MIRROR;
        BasePose.k.y *= MIRROR;
        BasePose.l.z *= MIRROR;

        joint->BasePose = BasePose;
      }
      else if(Scan.IfName(L"SI_Camera"))
      {
        ScanXSIName(dummy);
        Scan.Match('{');
        ScanFloat();  ScanFloat();  ScanFloat();
        ScanFloat();  ScanFloat();  ScanFloat();
        ScanFloat();  ScanFloat();  ScanFloat();
        ScanFloat();
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_Light"))
      {
        ScanXSIName(dummy);
        Scan.Match('{');
        sInt mode = ScanInt();
        ScanFloat(); ScanFloat(); ScanFloat();
        switch(mode)
        {
        case 0: // point or spot
          ScanFloat(); ScanFloat(); ScanFloat();
          break;
        case 1: // directional
          ScanFloat(); ScanFloat(); ScanFloat();
          break;
        case 2: // spot
          ScanFloat(); ScanFloat(); ScanFloat();
          ScanFloat(); ScanFloat(); ScanFloat();
          ScanFloat();
          ScanFloat();
          break;
        case 3: // xsi
          ScanFloat(); ScanFloat(); ScanFloat();
          ScanFloat(); ScanFloat(); ScanFloat();
          break;
        }
        if(Scan.IfName(L"SI_LightInfo"))
        {
          Scan.Match('{');
          ScanInt();

          ScanInt();
          ScanFloat();
          ScanFloat();
          ScanInt();
          ScanFloat();
          ScanInt();
          ScanFloat();
          ScanFloat();
          Scan.Match('}');
        }
        Scan.Match('}');
      }
      else if(Scan.IfName(L"XSI_CustomPSet"))
      {
        _CustomPSet();
      }
      else if(Scan.IfName(L"XSI_Mixer"))
      {
        ScanXSIName(dummy);
        Scan.Match('{');
        ScanInt();
        ScanInt();
        ScanInt();
        ScanInt();
        ScanInt();
        ScanInt();
        while(Scan.IfName(L"XSI_Action"))
        {
          ScanXSIName(dummy);
          Scan.Match('{');
          ScanFloat();
          ScanFloat();
          ScanInt();
          while(Scan.Token!='}' && !Scan.Errors)
          {
            if(Scan.IfName(L"XSI_StaticValues"))
            {
              Scan.Match('{');
              ScanInt();
              ScanString(dummy);
              ScanFloat();
              Scan.Match('}');
            }
            else if(Scan.IfName(L"SI_FCurve"))
            {
              _FCurveSkip();
            }
            else
            {
              SyntaxError();
            }
          }
          Scan.Match('}');
        }
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_Cluster"))
      {
        ScanXSIName(dummy);
        Scan.Match('{');
        ScanString(dummy);
        ScanString(dummy);
        ScanString(dummy);
        sInt max = ScanInt();
        for(sInt i=0;i<max;i++)
          ScanInt();
        if(Scan.IfName(L"XSI_ClusterInfo"))
        {
          Scan.Match('{');
          ScanString(dummy);
          Scan.Match('}');
        }
        while(Scan.IfName(L"XSI_CustomPSet"))
          _CustomPSet();
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_Model"))
      {
        sMatrix34 mat0 = Transform;
        sMatrix34 mat1 = BasePose;
        _Model(jointid,animated);
        Transform = mat0;
        BasePose = mat1;
      }
      else if(Scan.IfName(L"XSI_Mesh"))
      {
        Visible = visible;
        if(model->Mesh)
          Scan.Error(L"more than one mesh per model");
        _Mesh(model,jointid,animated);
      }
      else if(Scan.IfName(L"XSI_Camera"))
      {
        Scan.Match('{');
        ScanInt();
        ScanFloat();
        ScanFloat();
        ScanInt();
        ScanInt();
        ScanFloat();
        ScanFloat();
        ScanFloat();
        ScanFloat();
        ScanInt();
        ScanFloat();
        ScanFloat();
        ScanFloat();
        ScanFloat();
        if(Scan.IfName(L"XSI_CameraFocalLength"))
        {
          _DummySection();
        }
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_IK_Root") || Scan.IfName(L"SI_IK_Joint") || Scan.IfName(L"SI_IK_Effector"))
      {
        _DummySection();
      }
      else
      {
        SyntaxError();
      }
    }
    Scan.Match('}');

    if (model && model->Mesh)
      Models.AddTail(model);
  }

  /****************************************************************************/

  void XSILoader::_EnvelopeList()
  {
    sPoolString modelname,jointname,dummy;
    XSIModel *model;
//    Wz4MeshVertexPosition *pos;
    sInt joint;
    sInt n;


    Scan.Match('{');
    sInt envs = ScanInt();

    for(sInt i=0;i<envs;i++)
    {
      Scan.MatchName(L"SI_Envelope");
      ScanXSIName(dummy);
      Scan.Match('{');
      ScanString(modelname);
      n = sFindFirstChar(modelname,'.');
      if(n>=0)
      {
        sString<256> buffer;
        sSPrintF(buffer,L"MDL-%s",modelname+n+1);
        modelname = buffer;
      }
      model = sFind(Models,&XSIModel::Name,modelname);
     
      ScanString(jointname);
      n = sFindFirstChar(jointname,'.');
      if(n>=0)
      {
        sString<256> buffer;
        sSPrintF(buffer,L"MDL-%s",jointname+n+1);
        jointname = buffer;
      }
      joint = sFindIndex(MasterMesh->Skeleton->Joints,&Wz4AnimJoint::Name,jointname);
      if(joint==-1)
      {
        sDPrintF(L"unknown skeleton model <%s>\n",jointname);
  //      Scan.Error(L"unknown skeleton model <%s>",jointname);
  //      return;
      }


      sInt max = ScanInt();
      sInt index;
      sF32 value;

      for(sInt j=0;j<max && !Scan.Errors;j++)
      {
        index = ScanInt();
        value = ScanFloat();
        if(model && value>0 && joint!=-1)
        {
          for (sInt k=0; k<model->PositionMap.GetCount(); k++) if (model->PositionMap[k]==index)
            model->Mesh->Vertices[k].AddWeight(joint,value);
        }
      }
      Scan.Match('}');
    }
    Scan.Match('}');

    // rescale all weights
    sFORALL(Models,model)
    {
      Wz4MeshVertex *v;
      sFORALL(model->Mesh->Vertices,v)
        v->NormWeight();
    }
  }

  /****************************************************************************/

  void XSILoader::_Global()
  {
    sPoolString tpl,name,dummy;
    sTextBuffer tb;
    sInt version;
    sInt bits;

    MaterialsConnected  = 0;
    if(!Scan.IfName(L"xsi")) Scan.Error(L"header error");
    version = Scan.ScanInt();
    if(!Scan.IfName(L"txt")) Scan.Error(L"header error");
    bits = Scan.ScanInt();
    if(version<500 || bits!=32) Scan.Error(L"header error");

    while(!Scan.Errors && Scan.Token!=sTOK_END)
    {
      if(Scan.IfName(L"SI_FileInfo"))
      {
        Scan.Match('{');
        ScanString(dummy);
        ScanString(dummy);
        ScanString(dummy);
        ScanString(dummy);
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_Scene"))
      {
        Scan.Match('{');
        Scan.ScanString(dummy); Scan.Match(',');
        ScanFloat(); 
        ScanFloat(); 
        ScanFloat(); 
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_CoordinateSystem"))
      {
        Scan.Match('{');
        ScanInt();
        ScanInt();
        ScanInt();
        ScanInt();
        ScanInt();
        ScanInt();
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_Angle"))
      {
        Scan.Match('{');
        ScanInt();
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_Ambience"))
      {
        Scan.Match('{');
        ScanFloat(); 
        ScanFloat(); 
        ScanFloat(); 
        Scan.Match('}');
      }
      else if(Scan.IfName(L"SI_MaterialLibrary"))
      {
        _MaterialLibrary();
      }
      else if(Scan.IfName(L"XSI_ImageLibrary"))
      {
        _ImageLibrary();
      }
      else if(Scan.IfName(L"SI_Model"))
      {
        Transform.Init();
        _Model(-1,0);
      }
      else if(Scan.IfName(L"SI_EnvelopeList"))
      {
        _EnvelopeList();
      }
      /*
      else if(Scan.Token==sTOK_NAME)
      {
        Scan.ScanName(tpl);
        name = L"";
        if(Scan.Token==sTOK_NAME)
          ScanXSIName(name);
        Scan.ScanRaw(tb,'{','}');

        sDPrintF(L"unhandled template %s %s\n",tpl,name);
      }*/
      else
      {
        Scan.Error(L"XSI template expected");
      }
    }
  }

  /****************************************************************************/

  void XSILoader::FixAnimatedClusters()
  {
    Wz4MeshCluster *cl;
    Wz4MeshVertex *v;
    XSIModel *mod;

    sFORALL(Models,mod)
    {
      if(mod->JointId!=-1 && MasterMesh->Skeleton)
      {
        sInt animated = 0;

        sFORALL(mod->Mesh->Clusters,cl)
        {
          if(cl->Mtrl && !sCmpString(cl->Mtrl->Name+sGetStringLen(cl->Mtrl->Name)-5,L"-anim"))
            animated = 1;
        }

        if(animated)
        {
          sFORALL(mod->Mesh->Vertices,v)
          {
            if(v->Index[0]==-1)
            {
              v->Index[0] = mod->JointId;
              v->Index[1] = -1;
              v->Index[2] = -1;
              v->Index[3] = -1;
              v->Weight[0] = 1;
              v->Weight[1] = 0;
              v->Weight[2] = 0;
              v->Weight[3] = 0;
            }
          }
        }
        else
        {
          sMatrix34 mat;
          mat = MasterMesh->Skeleton->Joints[mod->JointId].fake;

          sFORALL(mod->Mesh->Vertices,v)
            v->Pos*=mat;
        }
      }
    }
  }

  /****************************************************************************/

  sBool XSILoader::Load(Wz4Mesh *master,const sChar *file)
  {
    Scan.Init();
    Scan.Flags |= sSF_UMLAUTS;
    Scan.DefaultTokens();
    Scan.StartFile(file);

    sInt n = 0;
    sInt max = sGetStringLen(file);
    for(sInt i=0;i<max;i++)
      if(file[i]=='/' || file[i]=='\\')
        n = i;
    Path.Init(file,n);

    MasterMesh = master;
    if(!MasterMesh->Skeleton)
      MasterMesh->Skeleton = new Wz4Skeleton;

    static sInt time = sGetTime();
    if(!Scan.Errors)
      _Global();
    FixAnimatedClusters();
    sDPrintF(L"loadxsi: %d ms\n",sGetTime()-time);

    // now coalesce the models into the master mesh
    XSIModel *model;
    sFORALL(Models, model)
      MasterMesh->Add(model->Mesh);

    MasterMesh->MergeClusters();
    MasterMesh->MergeVertices();

    sDeleteAll(Models);
    sReleaseAll(OutMaterials);
    sReleaseAll(OutTextures);

    if(MasterMesh->Skeleton)
      MasterMesh->Skeleton->FixTime(1/60.0f);

    MasterMesh->SplitClustersAnim(74);

    return Scan.Errors==0;
  }


}

/****************************************************************************/
/***                                                                      ***/
/***   Main                                                               ***/
/***                                                                      ***/
/****************************************************************************/

sBool Wz4Mesh::LoadXSI(const sChar *file,sBool forceanim,sBool forcergb)
{
  Wz4::XSILoader *load = new Wz4::XSILoader;
  load->ForceAnim = forceanim;
  load->ForceRGB = forcergb;
  sBool result = load->Load(this,file);
  delete load;
  return result;
}

/****************************************************************************/

