-- Nater's Tournament server (Supabase Postgres).
--
-- Tables live in the private schema `nt` (not exposed through the REST API).
-- The mod only calls the SECURITY DEFINER functions below through
-- POST /rest/v1/rpc/<name> with body {"a": {...}}; each returns jsonb.
-- Errors come back as {"error": "<code>"} with HTTP 200.

create schema if not exists nt;
revoke all on schema nt from public, anon, authenticated;

-- ---------------------------------------------------------------- tables

create table if not exists nt.tournaments (
  id            uuid primary key default gen_random_uuid(),
  code          text not null unique,
  name          text not null,
  is_public     boolean not null default true,
  gd            text not null,
  host_account  integer not null,
  host_name     text not null,
  host_hash     text not null,
  state         text not null default 'lobby'
                check (state in ('lobby', 'running', 'finished', 'closed')),
  max_players   integer not null default 16 check (max_players between 2 and 32),
  round         integer not null default 0,
  rounds        integer not null default 0,
  size          integer not null default 0,
  time_limit    integer not null default 300 check (time_limit between 30 and 3600),
  level_id      bigint,
  level_name    text,
  level_author  text,
  level_diff    integer,
  champion      integer,
  rev           bigint not null default 1,
  created_at    timestamptz not null default now(),
  last_activity timestamptz not null default now()
);
create index if not exists tournaments_list_idx
  on nt.tournaments (gd, is_public, state, last_activity desc);

create table if not exists nt.players (
  tournament_id uuid not null references nt.tournaments (id) on delete cascade,
  account_id    integer not null,
  user_id       integer not null default 0,
  name          text not null,
  icon          jsonb not null default '{}'::jsonb,
  secret_hash   text not null,
  seed          integer,
  status        text not null default 'in'
                check (status in ('in', 'out', 'left', 'kicked')),
  wins          integer not null default 0,
  best          real not null default 0,
  joined_at     timestamptz not null default now(),
  last_seen     timestamptz not null default now(),
  last_emote    timestamptz,
  primary key (tournament_id, account_id)
);

create table if not exists nt.matches (
  tournament_id uuid not null references nt.tournaments (id) on delete cascade,
  round       integer not null,
  slot        integer not null,
  p1          integer,
  p2          integer,
  state       text not null default 'pending' check (state in ('pending', 'live', 'done')),
  level_id    bigint,
  level_name  text,
  starts_at   timestamptz,
  ends_at     timestamptz,
  p1_best     real not null default 0,
  p2_best     real not null default 0,
  p1_best_at  timestamptz,
  p2_best_at  timestamptz,
  p1_cur      real not null default 0,
  p2_cur      real not null default 0,
  p1_att      integer not null default 0,
  p2_att      integer not null default 0,
  p1_seen     timestamptz,
  p2_seen     timestamptz,
  winner      integer,
  reason      text,
  finished_at timestamptz,
  primary key (tournament_id, round, slot)
);

create table if not exists nt.events (
  id            bigint generated always as identity primary key,
  tournament_id uuid not null references nt.tournaments (id) on delete cascade,
  kind          text not null,
  account_id    integer,
  data          jsonb not null default '{}'::jsonb,
  created_at    timestamptz not null default now()
);
create index if not exists events_tournament_idx on nt.events (tournament_id, id);

alter table nt.tournaments enable row level security;
alter table nt.players     enable row level security;
alter table nt.matches     enable row level security;
alter table nt.events      enable row level security;

-- ---------------------------------------------------------------- helpers

create or replace function nt.hash(s text) returns text
language sql immutable set search_path = '' as $$
  select encode(pg_catalog.sha256(pg_catalog.convert_to(coalesce(s, ''), 'UTF8')), 'hex')
$$;

create or replace function nt.ms(t timestamptz) returns bigint
language sql immutable set search_path = '' as $$
  select case when t is null then null else (extract(epoch from t) * 1000)::bigint end
$$;

-- printable ASCII only (GD's bitmap fonts), trimmed, capped
create or replace function nt.clean(s text, maxlen integer) returns text
language sql immutable set search_path = '' as $$
  select left(btrim(regexp_replace(coalesce(s, ''), '[^ -~]', '', 'g')), maxlen)
$$;

create or replace function nt.clampi(v jsonb, lo integer, hi integer, dflt integer) returns integer
language plpgsql immutable set search_path = '' as $$
declare x integer;
begin
  begin
    x := (v #>> '{}')::numeric::integer;
  exception when others then
    return dflt;
  end;
  if x is null then return dflt; end if;
  return greatest(lo, least(hi, x));
end $$;

create or replace function nt.clampf(v jsonb, lo real, hi real) returns real
language plpgsql immutable set search_path = '' as $$
declare x real;
begin
  begin
    x := (v #>> '{}')::real;
  exception when others then
    return lo;
  end;
  if x is null or x <> x then return lo; end if;
  return greatest(lo, least(hi, x));
end $$;

create or replace function nt.icon(v jsonb) returns jsonb
language sql immutable set search_path = '' as $$
  select jsonb_build_object(
    'c', nt.clampi(v -> 'c', 1, 999, 1),
    'p', nt.clampi(v -> 'p', 0, 255, 0),
    's', nt.clampi(v -> 's', 0, 255, 3),
    'g', nt.clampi(v -> 'g', -1, 255, -1)
  )
$$;

create or replace function nt.new_code() returns text
language plpgsql volatile set search_path = '' as $$
declare
  alphabet constant text := 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  c text;
begin
  loop
    c := '';
    for i in 1..5 loop
      c := c || substr(alphabet, 1 + floor(random() * 32)::integer, 1);
    end loop;
    exit when not exists (select 1 from nt.tournaments where code = c);
  end loop;
  return c;
end $$;

create or replace function nt.bitrev(x integer, bits integer) returns integer
language plpgsql immutable set search_path = '' as $$
declare r integer := 0;
begin
  for i in 1..bits loop
    r := (r << 1) | ((x >> (i - 1)) & 1);
  end loop;
  return r;
end $$;

create or replace function nt.event(tid uuid, k text, acc integer, d jsonb) returns void
language sql volatile set search_path = '' as $$
  insert into nt.events (tournament_id, kind, account_id, data) values (tid, k, acc, coalesce(d, '{}'::jsonb))
$$;

create or replace function nt.bump(tid uuid) returns void
language sql volatile set search_path = '' as $$
  update nt.tournaments set rev = rev + 1, last_activity = now() where id = tid
$$;

-- the caller's player row, if account + secret match
create or replace function nt.auth(tid uuid, a jsonb) returns nt.players
language sql stable set search_path = '' as $$
  select p.* from nt.players p
  where p.tournament_id = tid
    and p.account_id = nt.clampi(a -> 'account_id', 0, 2147483647, 0)
    and p.secret_hash = nt.hash(a ->> 'secret')
$$;

create or replace function nt.is_host(t nt.tournaments, a jsonb) returns boolean
language sql stable set search_path = '' as $$
  select t.host_account = nt.clampi(a -> 'account_id', 0, 2147483647, 0)
     and t.host_hash = nt.hash(a ->> 'secret')
$$;

-- ---------------------------------------------------------------- bracket

-- Ends a match and moves the winner on. Safe to call twice.
create or replace function nt.finish(tid uuid, r integer, s integer, w integer, why text) returns void
language plpgsql volatile set search_path = '' as $$
declare
  m nt.matches;
  t nt.tournaments;
  loser integer;
begin
  select * into m from nt.matches
    where tournament_id = tid and round = r and slot = s for update;
  if not found or m.state = 'done' then return; end if;
  if w is null or (w is distinct from m.p1 and w is distinct from m.p2) then return; end if;

  loser := case when w = m.p1 then m.p2 else m.p1 end;
  update nt.matches
     set state = 'done', winner = w, reason = why, finished_at = now()
   where tournament_id = tid and round = r and slot = s;

  if why <> 'bye' then
    update nt.players set wins = wins + 1 where tournament_id = tid and account_id = w;
  end if;
  update nt.players
     set best = greatest(best, case when account_id = m.p1 then m.p1_best else m.p2_best end)
   where tournament_id = tid and account_id in (m.p1, m.p2);
  if loser is not null then
    update nt.players set status = 'out'
     where tournament_id = tid and account_id = loser and status = 'in';
  end if;

  select * into t from nt.tournaments where id = tid for update;
  perform nt.event(tid, 'result', w, jsonb_build_object(
    'r', r, 's', s, 'w', w, 'l', loser, 'why', why,
    'b1', m.p1_best, 'b2', m.p2_best));

  if r >= t.rounds then
    update nt.tournaments
       set state = 'finished', champion = w, rev = rev + 1, last_activity = now()
     where id = tid;
    perform nt.event(tid, 'champion', w, jsonb_build_object('w', w));
  else
    if s % 2 = 0 then
      update nt.matches set p1 = w where tournament_id = tid and round = r + 1 and slot = s / 2;
    else
      update nt.matches set p2 = w where tournament_id = tid and round = r + 1 and slot = s / 2;
    end if;
    update nt.tournaments set rev = rev + 1, last_activity = now() where id = tid;
  end if;
end $$;

-- Decides a live match whose time is up (best %, then who got there first).
create or replace function nt.decide(m nt.matches) returns integer
language plpgsql volatile set search_path = '' as $$
begin
  if m.p1 is null then return m.p2; end if;
  if m.p2 is null then return m.p1; end if;
  if m.p1_best > m.p2_best then return m.p1; end if;
  if m.p2_best > m.p1_best then return m.p2; end if;
  if m.p1_best_at is not null and (m.p2_best_at is null or m.p1_best_at < m.p2_best_at) then return m.p1; end if;
  if m.p2_best_at is not null and (m.p1_best_at is null or m.p2_best_at < m.p1_best_at) then return m.p2; end if;
  return case when random() < 0.5 then m.p1 else m.p2 end;
end $$;

-- Lazy upkeep, run on reads: ends timed-out matches and no-shows.
-- Lock order everywhere: tournament row first, then match rows (no deadlocks).
create or replace function nt.upkeep(tid uuid) returns void
language plpgsql volatile set search_path = '' as $$
declare
  m nt.matches;
  gone1 boolean;
  gone2 boolean;
begin
  if not exists (select 1 from nt.matches
                  where tournament_id = tid and state = 'live'
                    and (now() > ends_at + interval '3 seconds' or now() > starts_at + interval '50 seconds')) then
    return;
  end if;
  perform 1 from nt.tournaments where id = tid for update;
  for m in
    select * from nt.matches
     where tournament_id = tid and state = 'live'
       and (now() > ends_at + interval '3 seconds' or now() > starts_at + interval '50 seconds')
  loop
    if now() > m.ends_at + interval '3 seconds' then
      perform nt.finish(tid, m.round, m.slot, nt.decide(m), 'time');
    else
      gone1 := m.p1_seen is null or m.p1_seen < now() - interval '45 seconds';
      gone2 := m.p2_seen is null or m.p2_seen < now() - interval '45 seconds';
      if gone1 and not gone2 then
        perform nt.finish(tid, m.round, m.slot, m.p2, 'forfeit');
      elsif gone2 and not gone1 then
        perform nt.finish(tid, m.round, m.slot, m.p1, 'forfeit');
      end if;
    end if;
  end loop;
end $$;

-- Starts the live matches of round r (both seats filled; empty or departed seats forfeit).
create or replace function nt.start_round(tid uuid, r integer) returns integer
language plpgsql volatile set search_path = '' as $$
declare
  t nt.tournaments;
  m nt.matches;
  st1 text;
  st2 text;
  ok1 boolean;
  ok2 boolean;
  started integer := 0;
  t0 timestamptz := now() + interval '9 seconds';
begin
  select * into t from nt.tournaments where id = tid for update;
  for m in select * from nt.matches where tournament_id = tid and round = r and state = 'pending' order by slot loop
    select status into st1 from nt.players where tournament_id = tid and account_id = m.p1;
    select status into st2 from nt.players where tournament_id = tid and account_id = m.p2;
    ok1 := m.p1 is not null and coalesce(st1, 'left') not in ('left', 'kicked');
    ok2 := m.p2 is not null and coalesce(st2, 'left') not in ('left', 'kicked');
    if ok1 and ok2 then
      update nt.matches
         set state = 'live', level_id = t.level_id, level_name = t.level_name,
             starts_at = t0, ends_at = t0 + make_interval(secs => t.time_limit),
             p1_best = 0, p2_best = 0, p1_cur = 0, p2_cur = 0, p1_att = 0, p2_att = 0,
             p1_best_at = null, p2_best_at = null, p1_seen = null, p2_seen = null
       where tournament_id = tid and round = r and slot = m.slot;
      started := started + 1;
    elsif ok1 then
      perform nt.finish(tid, r, m.slot, m.p1, case when m.p2 is null then 'bye' else 'forfeit' end);
    elsif ok2 then
      perform nt.finish(tid, r, m.slot, m.p2, case when m.p1 is null then 'bye' else 'forfeit' end);
    elsif m.p1 is not null then
      perform nt.finish(tid, r, m.slot, m.p1, 'forfeit');
    elsif m.p2 is not null then
      perform nt.finish(tid, r, m.slot, m.p2, 'forfeit');
    end if;
  end loop;
  return started;
end $$;

-- ---------------------------------------------------------------- views of state

create or replace function nt.tjson(t nt.tournaments) returns jsonb
language sql stable set search_path = '' as $$
  select jsonb_build_object(
    'code', t.code, 'name', t.name, 'pub', t.is_public, 'gd', t.gd,
    'host', t.host_account, 'hostName', t.host_name, 'state', t.state,
    'max', t.max_players, 'round', t.round, 'rounds', t.rounds, 'size', t.size,
    'time', t.time_limit, 'champ', t.champion, 'rev', t.rev,
    'level', case when t.level_id is null then null else jsonb_build_object(
      'id', t.level_id, 'name', t.level_name, 'author', t.level_author, 'diff', t.level_diff) end,
    'count', (select count(*) from nt.players p where p.tournament_id = t.id and p.status <> 'kicked' and p.status <> 'left')
  )
$$;

create or replace function nt.state_json(t nt.tournaments, since bigint, rev bigint) returns jsonb
language plpgsql stable set search_path = '' as $$
declare
  out jsonb;
  last_ev bigint;
begin
  out := jsonb_build_object('now', nt.ms(now()), 't', nt.tjson(t));

  if rev is distinct from t.rev then
    out := out || jsonb_build_object(
      'players', coalesce((select jsonb_agg(jsonb_build_object(
          'a', p.account_id, 'u', p.user_id, 'n', p.name, 'i', p.icon, 'seed', p.seed,
          'st', p.status, 'w', p.wins, 'b', p.best) order by p.joined_at)
        from nt.players p where p.tournament_id = t.id and p.status <> 'kicked'), '[]'::jsonb),
      'matches', coalesce((select jsonb_agg(jsonb_build_object(
          'r', m.round, 's', m.slot, 'p1', m.p1, 'p2', m.p2, 'st', m.state,
          'lv', m.level_id, 'ln', m.level_name, 'sa', nt.ms(m.starts_at), 'ea', nt.ms(m.ends_at),
          'w', m.winner, 'why', m.reason,
          'b1', m.p1_best, 'b2', m.p2_best, 'a1', m.p1_att, 'a2', m.p2_att) order by m.round, m.slot)
        from nt.matches m where m.tournament_id = t.id), '[]'::jsonb));
  end if;

  if t.state = 'running' then
    out := out || jsonb_build_object('live', coalesce((select jsonb_agg(jsonb_build_array(
        m.round, m.slot, m.p1_best, m.p2_best, m.p1_cur, m.p2_cur, m.p1_att, m.p2_att))
      from nt.matches m where m.tournament_id = t.id and m.state = 'live'), '[]'::jsonb));
  end if;

  select max(e.id) into last_ev from nt.events e where e.tournament_id = t.id;
  if since is null or since < 0 then
    out := out || jsonb_build_object('ev', '[]'::jsonb, 'evLast', coalesce(last_ev, 0));
  else
    out := out || jsonb_build_object('ev', coalesce((select jsonb_agg(x.j order by x.id) from (
        select e.id, jsonb_build_object('id', e.id, 'k', e.kind, 'a', e.account_id, 'd', e.data,
               't', nt.ms(e.created_at)) as j
          from nt.events e where e.tournament_id = t.id and e.id > since order by e.id limit 60) x),
        '[]'::jsonb),
      'evLast', coalesce(last_ev, 0));
  end if;
  return out;
end $$;

-- ---------------------------------------------------------------- public API

create or replace function public.nt_list(a jsonb) returns jsonb
language sql stable security definer set search_path = '' as $$
  select jsonb_build_object('now', nt.ms(now()), 'list', coalesce(jsonb_agg(x.j), '[]'::jsonb))
  from (
    select jsonb_build_object(
      'code', t.code, 'name', t.name, 'host', t.host_name, 'state', t.state,
      'max', t.max_players, 'round', t.round, 'rounds', t.rounds,
      'n', (select count(*) from nt.players p where p.tournament_id = t.id and p.status not in ('left', 'kicked')),
      'age', extract(epoch from now() - t.created_at)::integer) as j
    from nt.tournaments t
    where t.gd = coalesce(a ->> 'gd', '')
      and t.is_public
      and t.state in ('lobby', 'running', 'finished')
      and t.last_activity > now() - interval '6 hours'
      and (t.state <> 'finished' or t.last_activity > now() - interval '30 minutes')
    order by case t.state when 'lobby' then 0 when 'running' then 1 else 2 end, t.last_activity desc
    limit 50
  ) x
$$;

create or replace function public.nt_create(a jsonb) returns jsonb
language plpgsql volatile security definer set search_path = '' as $$
declare
  acc integer := nt.clampi(a -> 'account_id', 0, 2147483647, 0);
  uname text := nt.clean(a ->> 'username', 20);
  tname text := nt.clean(a ->> 'name', 32);
  sec text := coalesce(a ->> 'secret', '');
  gdv text := nt.clean(a ->> 'gd', 10);
  t nt.tournaments;
begin
  if acc <= 0 or uname = '' then return jsonb_build_object('error', 'account'); end if;
  if length(sec) < 16 then return jsonb_build_object('error', 'secret'); end if;
  if gdv not in ('2.2081', '2.2091') then return jsonb_build_object('error', 'version'); end if;
  if tname = '' then tname := uname || '''s Tournament'; end if;
  if (select count(*) from nt.tournaments
       where host_account = acc and state in ('lobby', 'running')
         and last_activity > now() - interval '6 hours') >= 3 then
    return jsonb_build_object('error', 'limit');
  end if;

  insert into nt.tournaments (code, name, is_public, gd, host_account, host_name, host_hash,
                              max_players, time_limit)
  values (nt.new_code(), tname, coalesce((a ->> 'public')::boolean, true), gdv, acc, uname, nt.hash(sec),
          nt.clampi(a -> 'max', 2, 32, 16), nt.clampi(a -> 'time', 30, 3600, 300))
  returning * into t;
  perform nt.event(t.id, 'created', acc, '{}'::jsonb);

  if coalesce((a ->> 'play')::boolean, true) then
    insert into nt.players (tournament_id, account_id, user_id, name, icon, secret_hash)
    values (t.id, acc, nt.clampi(a -> 'user_id', 0, 2147483647, 0), uname, nt.icon(a -> 'icon'), nt.hash(sec));
    perform nt.event(t.id, 'join', acc, jsonb_build_object('n', uname, 'i', nt.icon(a -> 'icon')));
    perform nt.bump(t.id);
    select * into t from nt.tournaments where id = t.id;
  end if;
  return jsonb_build_object('code', t.code) || nt.state_json(t, -1, null);
end $$;

create or replace function public.nt_join(a jsonb) returns jsonb
language plpgsql volatile security definer set search_path = '' as $$
declare
  t nt.tournaments;
  p nt.players;
  acc integer := nt.clampi(a -> 'account_id', 0, 2147483647, 0);
  uname text := nt.clean(a ->> 'username', 20);
  sec text := coalesce(a ->> 'secret', '');
  n integer;
begin
  select * into t from nt.tournaments where code = upper(nt.clean(a ->> 'code', 8)) for update;
  if not found or t.state = 'closed' then return jsonb_build_object('error', 'notfound'); end if;
  if t.gd <> coalesce(a ->> 'gd', '') then return jsonb_build_object('error', 'version', 'gd', t.gd); end if;
  if acc <= 0 or uname = '' then return jsonb_build_object('error', 'account'); end if;
  if length(sec) < 16 then return jsonb_build_object('error', 'secret'); end if;

  select * into p from nt.players where tournament_id = t.id and account_id = acc;
  if found then
    if p.secret_hash <> nt.hash(sec) then return jsonb_build_object('error', 'taken'); end if;
    if p.status = 'kicked' then return jsonb_build_object('error', 'kicked'); end if;
    if p.status = 'left' then
      if t.state <> 'lobby' then return jsonb_build_object('error', 'started'); end if;
      update nt.players set status = 'in', joined_at = now() where tournament_id = t.id and account_id = acc;
      perform nt.event(t.id, 'join', acc, jsonb_build_object('n', uname, 'i', nt.icon(a -> 'icon')));
    end if;
    update nt.players set name = uname, icon = nt.icon(a -> 'icon'), last_seen = now()
     where tournament_id = t.id and account_id = acc;
    perform nt.bump(t.id);
  else
    if t.state <> 'lobby' then return jsonb_build_object('error', 'started'); end if;
    select count(*) into n from nt.players where tournament_id = t.id and status = 'in';
    if n >= t.max_players then return jsonb_build_object('error', 'full'); end if;
    insert into nt.players (tournament_id, account_id, user_id, name, icon, secret_hash)
    values (t.id, acc, nt.clampi(a -> 'user_id', 0, 2147483647, 0), uname, nt.icon(a -> 'icon'), nt.hash(sec));
    perform nt.event(t.id, 'join', acc, jsonb_build_object('n', uname, 'i', nt.icon(a -> 'icon')));
    perform nt.bump(t.id);
  end if;

  select * into t from nt.tournaments where id = t.id;
  return nt.state_json(t, -1, null);
end $$;

create or replace function public.nt_leave(a jsonb) returns jsonb
language plpgsql volatile security definer set search_path = '' as $$
declare
  t nt.tournaments;
  p nt.players;
  m nt.matches;
begin
  select * into t from nt.tournaments where code = upper(nt.clean(a ->> 'code', 8)) for update;
  if not found then return jsonb_build_object('error', 'notfound'); end if;
  p := nt.auth(t.id, a);
  if p.account_id is null then return jsonb_build_object('error', 'auth'); end if;

  if t.state = 'lobby' then
    delete from nt.players where tournament_id = t.id and account_id = p.account_id;
  else
    update nt.players set status = 'left' where tournament_id = t.id and account_id = p.account_id and status = 'in';
    for m in select * from nt.matches
              where tournament_id = t.id and state = 'live' and p.account_id in (p1, p2) loop
      perform nt.finish(t.id, m.round, m.slot,
        case when m.p1 = p.account_id then m.p2 else m.p1 end, 'forfeit');
    end loop;
  end if;
  perform nt.event(t.id, 'leave', p.account_id, jsonb_build_object('n', p.name));
  perform nt.bump(t.id);
  return jsonb_build_object('ok', true);
end $$;

create or replace function public.nt_state(a jsonb) returns jsonb
language plpgsql volatile security definer set search_path = '' as $$
declare
  t nt.tournaments;
  acc integer := nt.clampi(a -> 'account_id', 0, 2147483647, 0);
begin
  select * into t from nt.tournaments where code = upper(nt.clean(a ->> 'code', 8));
  if not found or t.state = 'closed' then return jsonb_build_object('error', 'notfound'); end if;

  if t.state = 'running' then
    perform nt.upkeep(t.id);
    select * into t from nt.tournaments where id = t.id;
  end if;

  if acc > 0 then
    update nt.players set last_seen = now()
     where tournament_id = t.id and account_id = acc and secret_hash = nt.hash(a ->> 'secret')
       and last_seen < now() - interval '10 seconds';
  end if;

  return nt.state_json(t,
    case when a ? 'since' then nt.clampi(a -> 'since', -1, 2147483647, -1)::bigint else -1 end,
    case when a ? 'rev' then (a ->> 'rev')::bigint else null end);
end $$;

create or replace function public.nt_progress(a jsonb) returns jsonb
language plpgsql volatile security definer set search_path = '' as $$
declare
  t nt.tournaments;
  p nt.players;
  m nt.matches;
  r integer := nt.clampi(a -> 'round', 0, 99, 0);
  s integer := nt.clampi(a -> 'slot', 0, 999, 0);
  cur real := nt.clampf(a -> 'cur', 0, 100);
  best real := nt.clampf(a -> 'best', 0, 100);
  att integer := nt.clampi(a -> 'att', 0, 1000000, 0);
  done boolean := coalesce((a ->> 'done')::boolean, false);
  since bigint := nt.clampi(a -> 'since', 0, 2147483647, 0);
  me1 boolean;
begin
  select * into t from nt.tournaments where code = upper(nt.clean(a ->> 'code', 8)) for update;
  if not found then return jsonb_build_object('error', 'notfound'); end if;
  p := nt.auth(t.id, a);
  if p.account_id is null then return jsonb_build_object('error', 'auth'); end if;

  select * into m from nt.matches where tournament_id = t.id and round = r and slot = s;
  if not found or p.account_id is distinct from m.p1 and p.account_id is distinct from m.p2 then
    return jsonb_build_object('error', 'match');
  end if;
  me1 := m.p1 = p.account_id;

  if m.state = 'live' and now() >= m.starts_at - interval '2 seconds'
     and now() <= m.ends_at + interval '3 seconds' then
    if me1 then
      update nt.matches set
        p1_cur = cur, p1_att = greatest(p1_att, att), p1_seen = now(),
        p1_best_at = case when best > p1_best then now() else p1_best_at end,
        p1_best = greatest(p1_best, best)
      where tournament_id = t.id and round = r and slot = s;
    else
      update nt.matches set
        p2_cur = cur, p2_att = greatest(p2_att, att), p2_seen = now(),
        p2_best_at = case when best > p2_best then now() else p2_best_at end,
        p2_best = greatest(p2_best, best)
      where tournament_id = t.id and round = r and slot = s;
    end if;
    if done then
      if me1 then
        update nt.matches set p1_best = 100, p1_best_at = coalesce(p1_best_at, now())
         where tournament_id = t.id and round = r and slot = s and p1_best < 100;
      else
        update nt.matches set p2_best = 100, p2_best_at = coalesce(p2_best_at, now())
         where tournament_id = t.id and round = r and slot = s and p2_best < 100;
      end if;
      perform nt.finish(t.id, r, s, p.account_id, 'complete');
    end if;
  elsif m.state = 'live' then
    if me1 then
      update nt.matches set p1_seen = now() where tournament_id = t.id and round = r and slot = s;
    else
      update nt.matches set p2_seen = now() where tournament_id = t.id and round = r and slot = s;
    end if;
  end if;

  perform nt.upkeep(t.id);
  select * into m from nt.matches where tournament_id = t.id and round = r and slot = s;
  return jsonb_build_object(
    'now', nt.ms(now()), 'st', m.state, 'w', m.winner, 'why', m.reason,
    'sa', nt.ms(m.starts_at), 'ea', nt.ms(m.ends_at),
    'me', case when me1 then jsonb_build_object('b', m.p1_best, 'a', m.p1_att)
                        else jsonb_build_object('b', m.p2_best, 'a', m.p2_att) end,
    'opp', case when me1 then jsonb_build_object('b', m.p2_best, 'c', m.p2_cur, 'a', m.p2_att)
                         else jsonb_build_object('b', m.p1_best, 'c', m.p1_cur, 'a', m.p1_att) end,
    -- the opponent's emotes since the caller's last seen event (shown in the level HUD)
    'ev', case when since > 0 then coalesce((select jsonb_agg(jsonb_build_object(
              'id', e.id, 'k', e.kind, 'a', e.account_id, 'd', e.data) order by e.id)
            from (select * from nt.events x
                   where x.tournament_id = t.id and x.kind = 'emote' and x.id > since
                     and x.account_id = case when me1 then m.p2 else m.p1 end
                   order by x.id limit 10) e), '[]'::jsonb) else '[]'::jsonb end,
    'evLast', (select coalesce(max(x.id), 0) from nt.events x where x.tournament_id = t.id));
end $$;

create or replace function public.nt_emote(a jsonb) returns jsonb
language plpgsql volatile security definer set search_path = '' as $$
declare
  t nt.tournaments;
  p nt.players;
  e integer := nt.clampi(a -> 'emote', 1, 15, 1);
begin
  select * into t from nt.tournaments where code = upper(nt.clean(a ->> 'code', 8));
  if not found then return jsonb_build_object('error', 'notfound'); end if;
  p := nt.auth(t.id, a);
  if p.account_id is null then return jsonb_build_object('error', 'auth'); end if;
  if p.last_emote is not null and p.last_emote > now() - interval '1200 milliseconds' then
    return jsonb_build_object('error', 'slow');
  end if;
  update nt.players set last_emote = now() where tournament_id = t.id and account_id = p.account_id;
  perform nt.event(t.id, 'emote', p.account_id, jsonb_build_object('e', e));
  return jsonb_build_object('ok', true);
end $$;

create or replace function public.nt_host(a jsonb) returns jsonb
language plpgsql volatile security definer set search_path = '' as $$
declare
  t nt.tournaments;
  act text := coalesce(a ->> 'action', '');
  target integer := nt.clampi(a -> 'target', 0, 2147483647, 0);
  ids integer[];
  n integer;
  sz integer;
  nr integer;
  half integer;
  bits integer;
  i integer;
  m nt.matches;
  started integer;
begin
  select * into t from nt.tournaments where code = upper(nt.clean(a ->> 'code', 8)) for update;
  if not found or t.state = 'closed' then return jsonb_build_object('error', 'notfound'); end if;
  if not nt.is_host(t, a) then return jsonb_build_object('error', 'host'); end if;

  if act = 'start' then
    if t.state <> 'lobby' then return jsonb_build_object('error', 'state'); end if;
    select array_agg(account_id order by random()) into ids
      from nt.players where tournament_id = t.id and status = 'in';
    n := coalesce(array_length(ids, 1), 0);
    if n < 2 then return jsonb_build_object('error', 'few'); end if;
    sz := 2; nr := 1;
    while sz < n loop sz := sz * 2; nr := nr + 1; end loop;
    for r in 1..nr loop
      for s in 0..(sz >> r) - 1 loop
        insert into nt.matches (tournament_id, round, slot) values (t.id, r, s);
      end loop;
    end loop;
    half := sz / 2;
    bits := nr - 1;
    for i in 1..n loop
      if i <= half then
        update nt.matches set p1 = ids[i] where tournament_id = t.id and round = 1 and slot = i - 1;
      else
        update nt.matches set p2 = ids[i]
         where tournament_id = t.id and round = 1 and slot = nt.bitrev(i - half - 1, bits);
      end if;
      update nt.players set seed = i where tournament_id = t.id and account_id = ids[i];
    end loop;
    update nt.tournaments set state = 'running', round = 1, rounds = nr, size = sz,
           rev = rev + 1, last_activity = now() where id = t.id;
    select * into t from nt.tournaments where id = t.id;
    perform nt.event(t.id, 'start', null, jsonb_build_object('size', sz, 'rounds', nr));
    for m in select * from nt.matches where tournament_id = t.id and round = 1 and p2 is null and p1 is not null loop
      perform nt.finish(t.id, 1, m.slot, m.p1, 'bye');
    end loop;

  elsif act = 'level' then
    update nt.tournaments
       set level_id = nt.clampi(a -> 'level_id', 1, 2147483647, 1),
           level_name = nt.clean(a ->> 'level_name', 40),
           level_author = nt.clean(a ->> 'level_author', 20),
           level_diff = nt.clampi(a -> 'level_diff', 0, 20, 0),
           rev = rev + 1, last_activity = now()
     where id = t.id;
    perform nt.event(t.id, 'level', null, jsonb_build_object(
      'id', nt.clampi(a -> 'level_id', 1, 2147483647, 1), 'name', nt.clean(a ->> 'level_name', 40)));

  elsif act = 'round' then
    if t.state <> 'running' then return jsonb_build_object('error', 'state'); end if;
    if t.level_id is null then return jsonb_build_object('error', 'level'); end if;
    -- the current round must be finished before the next one starts
    if exists (select 1 from nt.matches where tournament_id = t.id and round = t.round and state = 'live') then
      return jsonb_build_object('error', 'busy');
    end if;
    if exists (select 1 from nt.matches where tournament_id = t.id and round = t.round and state = 'pending'
                 and (p1 is not null or p2 is not null)) then
      nr := t.round;
    elsif t.round < t.rounds then
      nr := t.round + 1;
    else
      return jsonb_build_object('error', 'state');
    end if;
    update nt.tournaments set round = nr where id = t.id;
    started := nt.start_round(t.id, nr);
    perform nt.bump(t.id);
    perform nt.event(t.id, 'round', null, jsonb_build_object('r', nr, 'n', started,
      'level', t.level_id, 'name', t.level_name));

  elsif act = 'kick' then
    if target = t.host_account then return jsonb_build_object('error', 'self'); end if;
    update nt.players set status = 'kicked' where tournament_id = t.id and account_id = target;
    if t.state <> 'lobby' then
      for m in select * from nt.matches
                where tournament_id = t.id and state = 'live' and target in (p1, p2) loop
        perform nt.finish(t.id, m.round, m.slot, case when m.p1 = target then m.p2 else m.p1 end, 'forfeit');
      end loop;
    end if;
    perform nt.event(t.id, 'kick', target, '{}'::jsonb);
    perform nt.bump(t.id);

  elsif act = 'win' then
    select * into m from nt.matches
     where tournament_id = t.id and round = nt.clampi(a -> 'round', 0, 99, 0) and slot = nt.clampi(a -> 'slot', 0, 999, 0);
    if not found or m.state = 'done' or target not in (coalesce(m.p1, 0), coalesce(m.p2, 0)) then
      return jsonb_build_object('error', 'match');
    end if;
    perform nt.finish(t.id, m.round, m.slot, target, 'host');

  elsif act = 'settings' then
    update nt.tournaments
       set is_public = coalesce((a ->> 'public')::boolean, is_public),
           time_limit = case when a ? 'time' then nt.clampi(a -> 'time', 30, 3600, 300) else time_limit end,
           max_players = case when a ? 'max' and state = 'lobby' then nt.clampi(a -> 'max', 2, 32, 16) else max_players end,
           name = case when a ? 'name' and nt.clean(a ->> 'name', 32) <> '' then nt.clean(a ->> 'name', 32) else name end,
           rev = rev + 1, last_activity = now()
     where id = t.id;

  elsif act = 'close' then
    update nt.tournaments set state = 'closed', rev = rev + 1, last_activity = now() where id = t.id;
    perform nt.event(t.id, 'close', null, '{}'::jsonb);
    return jsonb_build_object('ok', true);

  else
    return jsonb_build_object('error', 'action');
  end if;

  select * into t from nt.tournaments where id = t.id;
  return nt.state_json(t, -1, null);
end $$;

-- ---------------------------------------------------------------- grants

revoke all on all functions in schema nt from public, anon, authenticated;
revoke all on function public.nt_list(jsonb), public.nt_create(jsonb), public.nt_join(jsonb),
  public.nt_leave(jsonb), public.nt_state(jsonb), public.nt_progress(jsonb),
  public.nt_emote(jsonb), public.nt_host(jsonb) from public, authenticated;
grant execute on function public.nt_list(jsonb), public.nt_create(jsonb), public.nt_join(jsonb),
  public.nt_leave(jsonb), public.nt_state(jsonb), public.nt_progress(jsonb),
  public.nt_emote(jsonb), public.nt_host(jsonb) to anon;
