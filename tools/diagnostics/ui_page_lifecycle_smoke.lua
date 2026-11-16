-- Execute the production Lua adapter, including destruction between release
-- and asynchronous completion. The C++ API fixture cannot model Unity nulls.
local header = assert(io.open(arg[1] or "src/eiem_entry_ui_page.h", "rb"))
local text = header:read("*a")
header:close()
local source = assert(text:match('R"EFF_UI_PAGE%((.-)%)EFF_UI_PAGE"'), "adapter source missing")
local charClass, loader = {}, {}
local phase, item
function NotNull(value) return value ~= nil and value.alive ~= false end
HL = {
    Is = function(value, class) return value == item and class == charClass end,
    TryGet = function(value, name) return value[name] end
}
function require_ex() return {PhaseCharItem = charClass} end
CS = {UnityEngine = {AnimatorControllerParameterType = {Float = 1, Int = 2, Bool = 3}}}
local stack = {}
function stack:BottomIndex() return 1 end
function stack:TopIndex() return 1 end
function stack:Get() return phase end
PhaseManager = {GetPhaseStack = function() return stack end}

local function model(name)
    local go = {alive = true, name = name, layer = 13, activeSelf = true}
    local transform = {parent = {}, localPosition = {1, 2, 3}, localRotation = {0, 0, 0, 1}, localScale = {2, 2, 2}, sibling = 4}
    function transform:GetSiblingIndex() return self.sibling end
    function transform:SetSiblingIndex(value) self.sibling = value end
    function transform:SetParent(value) self.parent = value end
    local animator = {speed = 1.5, runtimeAnimatorController = {}, parameterCount = 1, layerCount = 1}
    function animator:GetParameter() return {type = 1, nameHash = 77} end
    function animator:GetFloat() return .75 end
    function animator:GetCurrentAnimatorStateInfo() return {fullPathHash = 99, normalizedTime = .25} end
    function animator:GetLayerWeight() return .8 end
    function animator:SetFloat(hash, value) self.float = value end
    function animator:SetLayerWeight(layer, value) self.weight = value end
    function animator:HasState() return true end
    function animator:Play(hash, layer, time) self.time = time end
    function animator:Update() end
    local mono = {}
    function mono:ChangeLayer(value) go.layer = value end
    function mono:LoadDecoItems() end
    function go:GetComponent(kind) return kind == "Animator" and animator or mono end
    go.transform = transform
    return go
end
local function setup()
    local old = model("old")
    item = {go = old, uiModelMono = old:GetComponent("CharUIModelMono"), animator = old:GetComponent("Animator"), charId = "test"}
    function item:BindGameObject(go)
        assert(NotNull(go), "attempted to bind a destroyed model")
        self.go, self.uiModelMono, self.animator = go, go:GetComponent("CharUIModelMono"), go:GetComponent("Animator")
    end
    function item:ReloadWeapon() end
    function item:SetVisible(value) self.go.activeSelf = value end
    phase = {modelLoader = loader, m_phaseItems = {[item] = true}}
    local factory = assert(loadstring(source))()
    return old, assert(factory(loader, old))
end
local old, bind = setup()
local pos, controller = old.transform.localPosition, old:GetComponent("Animator").runtimeAnimatorController
assert(bind(nil), "live validation failed")
assert(bind(nil, "release"), "release preparation failed")
old.alive = false -- UIModelLoader.UnloadGameObject before async completion.
local candidate = model("candidate")
assert(bind(candidate), "completed F10 rejected its page after unloading the old model")
assert(item.go == candidate and candidate.transform.localPosition == pos, "page transform was not handed off")
assert(item.animator.runtimeAnimatorController == controller and item.animator.time == .25 and item.animator.speed == 1.5, "animation state was not handed off")
assert(item.animator.float == .75 and item.animator.weight == .8, "animator parameters or layer state lost")
assert(bind(nil, "release"));candidate.alive = false
local second = model("second")
assert(bind(second) and item.go == second, "consecutive UI reload failed")
old, bind = setup();assert(bind(model("legacy")), "live transactional binding regressed")
old, bind = setup();assert(bind(nil, "release"));old.alive = false;phase.m_destroyed = true
assert(not bind(model("closed")), "closed page was resurrected")
old, bind = setup();assert(bind(nil, "release"));old.alive = false;item.go = model("external")
assert(not bind(model("stale")), "stale completion overwrote the game's new page owner")
print("PASS production UI page adapter: unloaded old model, async handoff, animator/transform state, consecutive reload, live binding and ended/replaced owners")
