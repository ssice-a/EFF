#pragma once
// The loader callback resumes a one-shot coroutine. Bind the existing page
// item through its own API instead; do not replay that coroutine or replace
// the Lua item which the page, formation cache and tick closures reference.
inline const char* UIPageFactorySource=R"EFF_UI_PAGE(
return function(loader, oldGo)
    if not PhaseManager or not NotNull(oldGo) then return nil end
    local charItemClass = require_ex("Phase/PhaseItem/PhaseCharItem").PhaseCharItem
    local stack = PhaseManager:GetPhaseStack()
    local phase, item
    for i = stack:BottomIndex(), stack:TopIndex() do
        local current = stack:Get(i)
        if current and not current.m_destroyed and current.modelLoader == loader then
            for candidate in pairs(current.m_phaseItems) do
                if HL.Is(candidate, charItemClass) and candidate.go == oldGo and candidate.uiModelMono and candidate.animator then
                    assert(not item, "ambiguous UI page owner")
                    phase, item = current, candidate
                end
            end
        end
    end
    if not item then return nil end
    return function(newGo)
        if phase.m_destroyed or not phase.m_phaseItems[item] or item.go ~= oldGo or not NotNull(oldGo) then return false end
        if newGo == nil then return true end
        local newMono = newGo:GetComponent("CharUIModelMono")
        local newAnimator = newGo:GetComponent("Animator")
        assert(NotNull(newMono) and NotNull(newAnimator), "candidate lost UI page components")
        local transform = oldGo.transform
        local parent, pos, rot, scale = transform.parent, transform.localPosition, transform.localRotation, transform.localScale
        local sibling, layer, visible, name = transform:GetSiblingIndex(), oldGo.layer, oldGo.activeSelf, oldGo.name
        local animator = item.animator
        local speed, controller = animator.speed, animator.runtimeAnimatorController
        local params, states = {}, {}
        for i = 0, animator.parameterCount - 1 do
            local p = animator:GetParameter(i)
            if p.type == CS.UnityEngine.AnimatorControllerParameterType.Float then
                params[#params + 1] = {"SetFloat", p.nameHash, animator:GetFloat(p.nameHash)}
            elseif p.type == CS.UnityEngine.AnimatorControllerParameterType.Int then
                params[#params + 1] = {"SetInteger", p.nameHash, animator:GetInteger(p.nameHash)}
            elseif p.type == CS.UnityEngine.AnimatorControllerParameterType.Bool then
                params[#params + 1] = {"SetBool", p.nameHash, animator:GetBool(p.nameHash)}
            end
        end
        for i = 0, animator.layerCount - 1 do
            local state = animator:GetCurrentAnimatorStateInfo(i)
            states[#states + 1] = {i, state.fullPathHash, state.normalizedTime, animator:GetLayerWeight(i)}
        end
        local view = item.m_tmpViewData
        local lightGroups = HL.TryGet(phase, "m_templateId2LightGroup")
        local lightGroup = lightGroups and lightGroups[item.charId]
        local function bind(go)
            item:BindGameObject(go)
            item.uiModelMono:ChangeLayer(layer)
            item.uiModelMono:LoadDecoItems()
            item:ReloadWeapon(true)
            if lightGroup then item.uiModelMono:InitLightFollower(lightGroup.go.transform) end
            if view then
                if view.weaponState then item:SwitchWeaponState(view.weaponState, view.ignoreStatic) end
                if view.integerName then item:SetInteger(view.integerName, view.integer) end
                if view.boolName then item:SetBool(view.boolName, view.bool) end
            end
            local target = item.animator
            target.runtimeAnimatorController = controller
            for _, p in ipairs(params) do target[p[1]](target, p[2], p[3]) end
            for _, s in ipairs(states) do
                target:SetLayerWeight(s[1], s[4])
                if s[2] ~= 0 and target:HasState(s[1], s[2]) then target:Play(s[2], s[1], s[3]) end
            end
            target.speed = speed
            target:Update(0)
            go.transform:SetParent(parent, false)
            go.transform.localPosition, go.transform.localRotation, go.transform.localScale = pos, rot, scale
            go.transform:SetSiblingIndex(sibling)
            go.name = name
            item:SetVisible(visible)
            assert(item.go == go and item.uiModelMono == go:GetComponent("CharUIModelMono") and item.animator == go:GetComponent("Animator"), "UI page binding did not transfer")
        end
        local ok, reason = pcall(bind, newGo)
        if not ok then
            local restored, restoreReason = pcall(bind, oldGo)
            assert(restored, "UI page rollback failed: " .. tostring(restoreReason))
            error(reason)
        end
        oldGo = newGo
        return true
    end
end
)EFF_UI_PAGE";
Ref uiPageFactory;
void BindUIPageApi(void* managerClass,void* functionClass,void* arrayClass,void* objectClass,void* exceptionClass){
 Require(objectClass,"UI page object structure unavailable");
 auto arraySet=Method(arrayClass,"SetValue",{"System.Object","System.Int32"},"System.Void",false);
 auto luaInstance=Method(managerClass,"get_instance",{},"Beyond.Lua.LuaManager",true);
 auto luaDo=Method(managerClass,"DoString",{"System.String"},"System.Object[]",false);
 auto luaCall=Method(functionClass,"Call",{"System.Object[]"},"System.Object[]",false);
 api.uiArraySet=arraySet;api.uiLuaInstance=luaInstance;api.uiLuaDo=luaDo;api.uiLuaCall=luaCall;api.uiObjectClass=objectClass;
 api.uiLuaExceptionMessage=Exact(exceptionClass,"get_Message",{},"System.String",false);
}
void BindUIPageCodec(void* cipherClass,void* cryptoClass,void* stringClass){
 auto key=Method(cipherClass,"GetKey",{},"System.String",true);
 auto encode=Method(cryptoClass,"EncryptToBase64String",{"System.String","System.String"},"System.String",true);
 auto decode=Method(cipherClass,"DecryptLuaString",{"System.String"},"System.String",true);
 auto equals=Method(stringClass,"Equals",{"System.String","System.String"},"System.Boolean",true);
 api.uiLuaKey=key;api.uiLuaEncode=encode;api.uiLuaDecode=decode;api.uiLuaCodeEquals=equals;
}
Ref CallUILua(const Ref& function,const std::vector<void*>& values){
 auto arguments=Ref::Make(il2cpp_array_new(api.uiObjectClass,values.size()),false);Require(arguments.Get(),"UI page argument allocation failed");
 for(size_t i=0;i<values.size();++i){int index=int(i);void* args[]={values[i],&index};Call(api.uiArraySet,arguments.Get(),args);}
 void* args[]={arguments.Get()};void* exception=nullptr;
 auto result=il2cpp_runtime_invoke(api.uiLuaCall,function.Get(),args,&exception);
 if(exception){
  auto exceptionRef=Ref::Make(exception,false);std::string message="managed exception";
  if(api.uiLuaExceptionMessage){void* formattingError=nullptr;auto text=Ref::Make(il2cpp_runtime_invoke(api.uiLuaExceptionMessage,exceptionRef.Get(),nullptr,&formattingError),false);
   if(!formattingError&&text.Get())message=String(text.Get());}
  throw std::runtime_error("game UI page Lua call failed: "+message);
 }
 return Ref::Make(result,false);
}
Ref CaptureUIPage(void* loader,void* root){
 Require(api.uiLuaInstance&&api.uiLuaDo&&api.uiLuaCall&&api.uiArraySet&&api.uiObjectClass&&api.uiLuaKey&&api.uiLuaEncode&&api.uiLuaDecode&&api.uiLuaCodeEquals,"game UI page API unavailable; original model retained");
 if(!uiPageFactory.Get()){
  auto manager=Ref::Make(Call(api.uiLuaInstance),false);Require(manager.Get(),"game LuaManager instance unavailable");
  // This game's loadbuffer always passes long chunks through LuaCypher. Use
  // its current key and codec; no fixed key, decryption hook or script files.
  auto code=Ref::Make(il2cpp_string_new(UIPageFactorySource),false);
  auto key=Ref::Make(Call(api.uiLuaKey),false);Require(key.Get(),"game Lua cipher key unavailable");
  void* encodeArgs[]={code.Get(),key.Get()};auto encoded=Ref::Make(Call(api.uiLuaEncode,nullptr,encodeArgs),false);Require(encoded.Get(),"game UI page adapter encoding failed");
  void* args[]={encoded.Get()};auto decoded=Ref::Make(Call(api.uiLuaDecode,nullptr,args),false);
  void* equalArgs[]={code.Get(),decoded.Get()};Require(RuntimeShapes::Scalar<unsigned char>(api.uiLuaCodeEquals,nullptr,equalArgs)!=0,"game UI page adapter codec round trip differs");
  auto output=Ref::Make(Call(api.uiLuaDo,manager.Get(),args),false);Require(output.Get(),"game Lua loader returned no page adapter values");
  auto result=ReferenceArray(output.Get());Require(result.size()==1&&result.front(),"game UI page adapter unavailable");uiPageFactory=Ref::Make(result.front(),false);
  Log("[FULL-RELOAD-UI-PAGE-LOAD] gameCodec=1 roundTripVerified=1 factoryReturned=1");
 }
 auto output=CallUILua(uiPageFactory,{loader,root});Require(output.Get(),"UI page lookup returned no values");auto result=ReferenceArray(output.Get());Require(result.size()==1&&result.front(),"UI loader instance has no game page item binding");
 return Ref::Make(result.front(),false);
}
bool BindUIPage(const Ref& binding,void* root=nullptr){
 auto output=CallUILua(binding,{root});Require(output.Get(),"UI page binding returned no values");auto result=ReferenceArray(output.Get());Require(result.size()==1&&result.front(),"UI page binding returned no result");
 auto klass=il2cpp_object_get_class(result.front());Require(klass&&strcmp(il2cpp_class_get_name(klass),"Boolean")==0&&strcmp(il2cpp_class_get_namespace(klass),"System")==0,"UI page binding returned a different type");
 unsigned char accepted=0;Require(CopyRaw(il2cpp_object_unbox(result.front()),&accepted,sizeof(accepted)),"UI page binding result unavailable");return accepted!=0;
}
