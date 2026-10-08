-- Run the exact embedded page adapter against game lifecycle seams. The
-- one-shot continuation is deliberately unable to update an existing item.
local f = assert(io.open(arg[1], "rb")); local header = f:read("a"); f:close()
local source = assert(header:match('R"EFF_UI_PAGE%((.-)%)EFF_UI_PAGE"'))
local factory = assert(load(source, "@production-ui-page-adapter"))()
CS = {UnityEngine = {AnimatorControllerParameterType = {Float = 1, Int = 2, Bool = 3}}}
local charItemClass = {}
function require_ex(path) assert(path == "Phase/PhaseItem/PhaseCharItem"); return {PhaseCharItem = charItemClass} end
HL = {Is = function(item, class) return class == charItemClass and rawget(item, "_class") == charItemClass end,
    TryGet = function(item, name) return rawget(item, name) end}
function NotNull(value) return value ~= nil and not value.destroyed end
local function model(name)
    local a = {parameterCount = 3, layerCount = 2, speed = 1.25, runtimeAnimatorController = "controller", values = {[10] = 0.6, [20] = 3, [30] = true}, weights = {[0] = 1, [1] = 0.7}}
    function a:GetParameter(i) return {type = i + 1, nameHash = (i + 1) * 10} end
    function a:GetFloat(hash) return self.values[hash] end
    a.GetInteger, a.GetBool = a.GetFloat, a.GetFloat
    function a:SetFloat(hash, value) self.values[hash] = value end
    a.SetInteger, a.SetBool = a.SetFloat, a.SetFloat
    function a:GetCurrentAnimatorStateInfo(i) return {fullPathHash = 100 + i, normalizedTime = 0.35 + i} end
    function a:GetLayerWeight(i) return self.weights[i] end
    function a:SetLayerWeight(i, value) self.weights[i] = value end
    function a:HasState(_, hash) return hash >= 100 end
    function a:Play(hash, layer, time) self.played = self.played or {}; self.played[layer] = {hash, time} end
    function a:Update(_) self.updates = (self.updates or 0) + 1 end
    local go = {name = name, layer = 17, activeSelf = true, animator = a}
    go.transform = {parent = "moved-parent", localPosition = "position", localRotation = "rotation", localScale = "scale", sibling = 2}
    function go.transform:GetSiblingIndex() return self.sibling end
    function go.transform:SetSiblingIndex(i) self.sibling = i end
    function go.transform:SetParent(parent) self.parent = parent end
    go.mono = {}
    function go.mono:ChangeLayer(layer) go.layer = layer end
    function go.mono:LoadDecoItems() self.deco = true end
    function go.mono:InitLightFollower(root) self.light = root end
    function go:GetComponent(name) return name == "Animator" and self.animator or name == "CharUIModelMono" and self.mono or nil end
    return go
end
local loader = {}
local function setup(old, bottom)
    bottom = bottom or 1
    local item = {_class = charItemClass, go = old, animator = old.animator, uiModelMono = old.mono, charId = "runtime-character", m_tmpViewData = {weaponState = "shown", integerName = "state", integer = 2, boolName = "paused", bool = true}}
    function item:BindGameObject(go)
        self.go, self.animator, self.uiModelMono, self.view = go, go.animator, go.mono, {root = go}
        self.m_tmpViewData = {}
        self.animator.speed = 1; self.animator:Update(0)
    end
    function item:ReloadWeapon(force) assert(force); self.uiModelMono.weapon = true end
    function item:SwitchWeaponState(state) self.m_tmpViewData.weaponState = state end
    function item:SetInteger(name, value) self.m_tmpViewData.integerName, self.m_tmpViewData.integer = name, value end
    function item:SetBool(name, value) self.m_tmpViewData.boolName, self.m_tmpViewData.bool = name, value end
    function item:SetVisible(value) self.go.activeSelf = value end
    -- Real phase sets contain PhasePanelItem (which has no go field), scenery
    -- and light items as well as characters. HyperLua rejects undeclared reads.
    local panel = setmetatable({}, {__index = function(_, key) error("HyperLua undeclared panel member: " .. key) end})
    local scenery = {go = model("scenery")}
    local phase = {modelLoader = loader, m_destroyed = false, m_phaseItems = {[panel] = true, [scenery] = true, [item] = true}, m_templateId2LightGroup = {['runtime-character'] = {go = {transform = "actual-light-root"}}}}
    local stack = {[bottom] = phase}
    function stack:Count() return 1 end
    function stack:Get(i) return self[i] end
    function stack:BottomIndex() return bottom end
    function stack:TopIndex() return bottom end
    PhaseManager = {GetPhaseStack = function() return stack end}
    return item, phase
end
do
    local old, new = model("old"), model("new")
    local item = setup(old); local finished = true
    local function continuation(go) if not finished then item:BindGameObject(go); finished = true end end
    continuation(new) -- native loader accepts new, but the coroutine is done.
    old.destroyed = true
    assert(item.go == old and not NotNull(item.go), "one-shot page blank repro was lost")
end
for _, visible in ipairs({true, false}) do
    local old, new = model("original"), model("candidate")
    old.activeSelf = visible
    local item = setup(old, visible and 1 or 4); local binding = assert(factory(loader, old)); assert(binding(nil))
    assert(binding(new) and item.go == new and item.view.root == new)
    assert(new.layer == 17 and new.activeSelf == visible and new.name == "original")
    assert(new.transform.parent == "moved-parent" and new.transform.localPosition == "position" and new.transform.localScale == "scale" and new.transform.sibling == 2)
    assert(new.animator.speed == 1.25 and new.animator.values[10] == 0.6 and new.animator.values[20] == 3 and new.animator.values[30])
    assert(new.animator.played[1][1] == 101 and new.animator.played[1][2] == 1.35)
    assert(new.mono.weapon and new.mono.deco and new.mono.light == "actual-light-root" and item.m_tmpViewData.weaponState == "shown")
    local next = model("next"); assert(binding(next) and item.go == next and next.activeSelf == visible)
end
do
    local old, new = model("old"), model("bad")
    local item = setup(old); local binding = assert(factory(loader, old)); new.mono = nil
    assert(not pcall(binding, new) and item.go == old, "missing candidate components changed page ownership")
    new = model("failing"); function new.mono:LoadDecoItems() error("native init failure") end
    assert(not pcall(binding, new) and item.go == old and item.animator == old.animator, "page rollback lost old references")
end
do
    local old = model("old"); local item, phase = setup(old); local binding = assert(factory(loader, old))
    phase.m_phaseItems[item] = nil; assert(not binding(nil) and not binding(model("new")))
    assert(factory({}, old) == nil, "another loader was accepted")
end
do
    local old, new = model("old"), model("new"); local item, phase = setup(old)
    phase.m_templateId2LightGroup = nil
    setmetatable(phase, {__index = function(_, key) error("HyperLua undeclared phase member: " .. key) end})
    local binding = assert(factory(loader, old)); assert(binding(new) and item.go == new and not new.mono.light)
end
print("PASS game UI page adapter: mixed strict HyperLua page items; real stack bounds and optional fields; one-shot continuation blank reproduced; actual item binding, repeat reload, moved parent, cached visibility, layer/pose/animation/weapon/light state, cancellation and rollback")
